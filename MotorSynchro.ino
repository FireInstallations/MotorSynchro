/*This is a program for speed and phase regulation of 4 motors equipped with indexer washers and reflex couplers
   the motors are driven by PWM Pins of the arduino and amplified by npn transistors
  Transistors:
  M1 via Pin10 Transistor oben rechts
  M2 via Pin09 Transistor oben links
  M3 via Pin05 Transistor links unten
  M1 via Pin06 Transistor rechts unten

  Schmidt Trigger:
  MOS4093 Pin07 GND
  MOS4093 Pin01 5V
  MOS4093 Pin05 5V
  MOS4093 Pin08 5V
  MOS4093 Pin12 5V
  MOS4093 Pin14 5V
  RK1 in an MOS4093 Pin02 via MOS4093 Pin03 an Arduino Pin02
  RK2 in an MOS4093 Pin06 via MOS4093 Pin04 an Arduino Pin03
  RK3 in an MOS4093 Pin09 via MOS4093 Pin10 an Arduino Pin08
  RK4 in an MOS4093 Pin13 via MOS4093 Pin11 an Arduino Pin12

  Poti A0
  Stroboskop Arduino Pin07

  switch lock/unlock Arduino Pin11 lock-HIGH, unlock-LOW

*/

//arrays of arduino pins
const byte ReadPins[]  = {2, 3, 8, 12};// reflex sensors 0 to 3
const byte WritePins[] = {10, 9, 5, 6};// motors 0 to 3

enum ConsoleCommands {InSpeedC, DecSpeedC, SpeedC, FreqCC, PhaseCC, PotC, RotCC, MSpeedC, HelpC, AutoC};
bool AutoMode = true;
byte AutoState = 0;
byte StrobCnt = 0;
bool WPFlagOld = true;

// actual rotor states
bool StateNow[4] = {false, false, false, false};
//last Rotorstates
bool LastState[4];
// is used to count ramps for frequency determination: 1 - first ramp found, 2 - second ramp found (halfperiod), 3 - third ramp found (full period)
byte GotPeriod[4] = {0, 0, 0, 0};

// actual speeds
byte ASpeed [4] = {50, 50, 50, 50};
byte ASpeedDD [4] = {0, 0, 0, 0}; // decimal digits
// Slave Speeds memory, which is filled out in DisplaySpeeds, index = 0 means deceleration speed, index = 1 normal speed and index = 2 accelerated speed
byte SSpeed[3][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}}; // [column][row] {{column},{row, row, row}, ...}
byte SSpeedDD[3][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}}; // [rotor][index]
float LastSpeed = 20;// for comparison of Potentiometer given speeds
float LastPotiTest = 20;
long LastPotiTime = 0;


//quarter period in microseconds
long RotorCount[4] = {0, 0, 0, 0};


//strings that contain the slave highs & lows as plus and minus
String SlaveString[3] = {"", "", ""};
//Counts loop  % 100 for SlaveString
byte LoopCount = 0;

bool PotiFlag = false;// enables potiinput
bool CountFinished = false;// indicates, if counting for frequency measurement is finished
bool FCFlag = false;// indicates frequency control
bool PCFlag = false;// indicates phase control
bool DFlag = true;// display Strings of Slaves
bool WPFlag = false; // White Period Flag of the master
long FCTimer = 0;// timer for frequency control

void setup() {
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(12, INPUT);//RK4
  pinMode(8, INPUT); //RK3
  pinMode(3, INPUT); //RK2
  pinMode(2, INPUT); //RK1
  pinMode(11, INPUT);// lock (HIGH) unlock (LOW) Pin
  pinMode(10, OUTPUT);//M4
  pinMode(9, OUTPUT); //M3
  pinMode(6, OUTPUT); //M2
  pinMode(5, OUTPUT); //M1
  pinMode(A0, INPUT); //Poti
  pinMode(7, OUTPUT); //Stroboskop
  Serial.begin(9600);

  // some time for startup
  delay(2000);

  //if they should rotate, start them once
  //if (GetPotiSSpeed () > 14) {
  Serial.println("Starting.");

  LastSpeed = analogRead(A0) * 50.0 / 1023.0;

  // and give all master SSpeed

  if (AutoMode) {
    AutoState = 0;
  }
  LastPotiTest = GetPotiSSpeed();

  delay(200);
  FCTimer = millis();
  LastPotiTime = millis();
}

void loop() {

  bool lock = digitalRead(11);
  if (lock) {
    if (PCFlag)digitalWrite(LED_BUILTIN, HIGH); else digitalWrite(LED_BUILTIN, LOW);

    //write down just every 100th loop
    LoopCount++;
    if (LoopCount == 100)LoopCount = 0;
    // realize intermediate values between SSpeed and ShadowSSpeed with loop counter
    for (byte i = 0; i < 4; i++) {

      if (!LoopCount)
        analogWrite(WritePins[i], ASpeed[i] + 1);
      if (LoopCount == ASpeedDD[i])
        analogWrite(WritePins[i], ASpeed[i]);

    }// end of for loop

    //Get serial
    GetSerial();
    //Get potiSSpeed if Potiflag

    // check for new poti input every sec
    if (PotiFlag) {
      if (millis() - LastPotiTime > 1000) {
        LastPotiTime = millis();

        float NewSpeed = GetPotiSSpeed();// gets the potiSSpeed for master

        if (abs(NewSpeed - LastPotiTest) > 5 ) {

          if (NewSpeed < 15) {
            for (int i = 0; i < 4; i++)
              SpeedSetter (&ASpeed[i], &ASpeedDD[i], 0.0);
          } else {
            SpeedSetter (&ASpeed[0], &ASpeedDD[0], NewSpeed);
          }
          AutoState = 0;

          LastPotiTest = NewSpeed;
        }
      }
    }

    //AutoMode
    //0: nothing has happend before, so set all salves to master speed and reset all flags
    //1: start frequency control and reset flags in case they have changed
    //2: wait for frequency control to finish
    //3: start phase control (we are basically done)
    if (AutoMode) {
      switch (AutoState) {
        case 0:
          DoCommands(PotC, true, 0);
          DoCommands(FreqCC, false, 0);
          DoCommands(PhaseCC, false, 0);

          DoCommands(MSpeedC, 0, 0);
          AutoState = 1;
          break;
        case 1:
          DoCommands(PhaseCC, false, 0);

          DoCommands(FreqCC, true, 0);
          AutoState = 2;
          break;
        case 2:
          if (!FCFlag) {
            DoCommands(PhaseCC, true, 0);
            AutoState = 3;
          }
          break;
      }
    }

    // frequency control

    if (millis() - FCTimer > 2000 && FCFlag && CountFinished) {
      GetRotorCounts();

      for (byte i = 1; i < 4; i++) {
        if (RotorCount[i] > RotorCount[0])
          SpeedAdder(&ASpeed[i], &ASpeedDD[i], 0.2);

        else if (RotorCount[i] < RotorCount[0])
          SpeedAdder(&ASpeed[i], &ASpeedDD[i], -0.2);

      }
    }
    // finish condition of frequency control
    if (FCFlag) {
      if (( abs((long)RotorCount[0] - (long)RotorCount[1])  / (float)RotorCount[0]  ) < 0.05 &&
          ( abs((long)RotorCount[0] - (long)RotorCount[2])  / (float)RotorCount[0]  ) < 0.05 &&
          ( abs((long)RotorCount[0] - (long)RotorCount[3])  / (float)RotorCount[0]  ) < 0.05 ) {
        Serial.println("frequency locked");
        DisplaySpeeds();
        FCFlag = false;
      }
    }

    if (PCFlag) {
      

      // generating the strobe signal for every eights white period, pulse length is the white period
      if (WPFlag != WPFlagOld) {
        //Serial.println(StrobCnt);
        StrobCnt++, StrobCnt %= 8;
        WPFlagOld = WPFlag;
        if (!StrobCnt) {
          digitalWrite(7, HIGH);
        //  Serial.print("Blink");

        } else
          digitalWrite(7, LOW);
      }
      // store all states until the master is white
      if (!WPFlag)// if the master is black
        for (byte i = 0; i < 4; i++) {
          LastState[i] = digitalRead(ReadPins[i]);

          if (LastState[0])// ignore the first time, when master turned to white, so the new master state is recognized in the following
            LastState[0] = false;
        }

      if (DFlag && !LoopCount) {// if display is active, loop counter = 0
        StateNow[0] = digitalRead(ReadPins[0]);// look for a new Master state
        if (StateNow[0] != LastState[0]) {// Master has changed
          if (!LastState[0]) {
            WPFlagOld = false;
            WPFlag = true;// if the last Master state was black, it is white now
            LastState[0] = StateNow[0];// store the actual white Master state
          }
          else WPFlag = false;// if the last Master state was white, it is black now
        }
        if (WPFlag) { // it is the white period of Master

          for (byte i = 1; i < 4; i++) {
            // read slave stati
            bool TempState = digitalRead(ReadPins[i]);

            // write the slave strings
            if ( TempState)
              SlaveString[i - 1] += "+"; else SlaveString[i - 1] += "-";

            if (TempState != StateNow[i]) {
              StateNow[i] = TempState;
              if (PCFlag) {// if Phase Control is active
                // regulate the phases of the slaves

                if (LastState[i] == StateNow[i] && !StateNow[i]) {// accelerate as long as leading minusses occur
                  ASpeed[i] = SSpeed[i - 1][2], ASpeedDD[i] = SSpeedDD[i - 1][2];
                  //Serial.println((String)(i + 1) + " accelerated");
                }
                else if (StateNow[i]) { //  normal slave speed
                  ASpeed[i] = SSpeed[i - 1][1], ASpeedDD[i] = SSpeedDD[i - 1][1];
                  //Serial.println((String)(i + 1) + " normelized");
                }
                else if (!StateNow[i]) {// decelerate if trailing minusses occur
                  ASpeed[i] = SSpeed[i - 1][0], ASpeedDD[i] = SSpeedDD[i - 1][0];

                  //Serial.println((String)(i + 1) + " decelerated");
                }
              }
            }

          }

        } else if (LastState[0]) {// in the next loop LastState[0] is updated and is low, so it is done only once
          // the black period of Master has started, print out the slave string and clear it
          //return to normal slave speeds
          for (byte i = 1; i < 4; i++)
            ASpeed[i] = SSpeed[i - 1][1], ASpeedDD[i] = SSpeedDD[i - 1][1];

          // display results
          if (DFlag) {
            // print out the normal speeds of all rotors
            //DisplaySpeeds();
            // print out the strings
            for (byte i = 0; i < 3; i++) {
              Serial.print(i + 2);
              Serial.print (" :  ");
              Serial.println(SlaveString[i]);

              SlaveString[i] = "";
            }
            Serial.println();
            Serial.println();
          }
        }
      }
    }
  }
  else
    digitalWrite(LED_BUILTIN, LOW);   // turn the LED on (HIGH is the voltage level)

}

//***************************** subroutines ********************************************************************

// calculates a specific number of a given string.
// every string a other number.
// hardcoded strings will be calculated while compilation
constexpr
unsigned int hash(const char* str, int h = 0)
{
  return !str[h] ? 5381 : (hash(str, h + 1) * 33) ^ str[h];
}

float GetPotiSSpeed () {//get SSpeed for the master from potentiometer
  float NewSpeed = analogRead(A0) * 67.0 / 1023.0;

  if ( abs(NewSpeed - LastSpeed) > 3) {
  }

  NewSpeed = NewSpeed * 0.1 + LastSpeed * 0.9;
  LastSpeed = NewSpeed;

  return NewSpeed;
}

int GetMotorNum(String instring, int CommandEnd) {
  //get motor number
  int Mnum = (int)instring[CommandEnd + 1];

  //cast asciinum to int
  Mnum -= 49;
  //if no number was given we get the ascii value of s -50 (65) , so we set Mnum to 0 (default)
  if (Mnum > 3 || Mnum < 0)
    Mnum = 0;

  return Mnum;
}

//Reads input from Serial and finds the given commands and values
void GetSerial () {// commands over serial monitor
  //!ToDo the serial monitor dosn't must have lineend

  if (Serial.available() > 0) {
    // read the incoming String:
    int i = 0;
    String instring = Serial.readString();
    String Command = instring;
    instring.toLowerCase();

    //echo
    Serial.println("Given input: " +  instring);

    // if space was given a value will be expected
    int CommandEnd = instring.indexOf(" ");
    if (CommandEnd > -1) //remove value(s) from command
      Command = instring.substring(0, CommandEnd);

    switch (hash(Command.c_str()) ) {//switch though commands
      case hash("i"): //increase speed
        i = GetMotorNum(instring, CommandEnd) - 1;

        instring = instring.substring(instring.lastIndexOf(' ') + 1, instring.length());
        DoCommands(InSpeedC, i,  instring.toFloat());
        break;
      case hash("d"): //decrease speed
        i = GetMotorNum(instring, CommandEnd) - 1;

        instring = instring.substring(instring.lastIndexOf(' ') + 1, instring.length());
        DoCommands(DecSpeedC, i, instring.toFloat());
        break;

      case hash("s"): //set speed
        i = GetMotorNum(instring, CommandEnd);

        instring = instring.substring(instring.lastIndexOf(' ') + 1, instring.length());
        DoCommands(SpeedC, i, instring.toFloat());
        break;

      case hash("po"): //set poti flag
        if (CommandEnd > -1)
          DoCommands(PotC, instring[CommandEnd + 1] != '-', 0);
        else
          DoCommands(PotC, true, 0);
        break;

      case hash("fc"): //set freqency flag
        if (CommandEnd > -1)
          DoCommands(FreqCC, instring[CommandEnd + 1] != '-', 0);
        else
          DoCommands(FreqCC, true, 0);
        break;

      case hash("pc"): //set phase flag
        if (CommandEnd > -1)
          DoCommands(PhaseCC, instring[CommandEnd + 1] != '-', 0);
        else
          DoCommands(PhaseCC, true, 0);
        break;

      case hash("au"): //set AutoMode flag
        if (CommandEnd > -1)
          DoCommands(AutoC, instring[CommandEnd + 1] != '-', 0);
        else
          DoCommands(AutoC, true, 0);
        break;

      case hash("c"): //print Rotorcounts
        DoCommands(RotCC, 0, 0);
        break;

      case hash("a"): //set all speeds to master
        DoCommands(MSpeedC, 0, 0);
        break;

      case hash("help"): //print help
        DoCommands(HelpC, 0, 0);
        break;

      default: //is it a speedvalue for master?
        if (isdigit(instring[0])) {
          SpeedSetter(&ASpeed[0],  &ASpeedDD[0], instring.toFloat());
          DisplaySpeeds();

          //start with 0 again so we count right
          LoopCount = 0;
        } else //or nothing?
          Serial.println("Didn't understand \"" + instring + "\".");
        Serial.println("Might want to use the command \"help\" ?");
        break;
    }
  }
}

//do the commands from Serial input or automode
// all commands are assigned to a certain int via enum ConsoleCommands
// value1 acts as motor index for speeds, while value2 is speed, and bool for flags
void DoCommands (byte Command, byte value1, float value2) {
  switch (Command) {
    case InSpeedC: //increase speed
      SSpeed[value1][2] = SSpeed[value1][1];
      SSpeedDD[value1][2] = SSpeedDD[value1][1];

      SpeedAdder(&SSpeed[value1][2], &SSpeedDD[value1][2], value2);

      Serial.print(SSpeed[value1][2]);
      Serial.print( ".");
      if (SSpeedDD[value1][2] < 10)
        Serial.print(0);
      Serial.println(SSpeedDD[value1][2]);
      break;

    case DecSpeedC: //decrease speed
      SSpeed[value1][0] = SSpeed[value1][1];
      SSpeedDD[value1][0] = SSpeedDD[value1][1];

      SpeedAdder(&SSpeed[value1][0], &SSpeedDD[value1][0], value2);

      Serial.print(SSpeed[value1][0]);
      Serial.print( ".");
      if (SSpeedDD[value1][2] < 10)
        Serial.print(0);
      Serial.println(SSpeedDD[value1][0]);
      break;

    case SpeedC: //set speed (value2) of motor value1
      SpeedSetter(&ASpeed[value1], &ASpeedDD[value1], value2);
      DisplaySpeeds();
      break;

    case FreqCC:
      FCFlag = (bool)value1;
      CountFinished = (bool)value1;
      Serial.println("FCFlag = " + (String)FCFlag);
      break;

    case PhaseCC:
      PCFlag = (bool)value1;
      Serial.println("PCFlag = " + (String)PCFlag);
      break;

    case PotC:
      PotiFlag = (bool)value1;
      Serial.println("PotiFlag = " + (String)PotiFlag);
      break;

    case RotCC:
      GetRotorCounts();
      break;

    case MSpeedC: //set all to master speed;
      for (byte i = 1; i < 4 ; i++) {
        ASpeed[i] = ASpeed[0]; //MasterSpeed
        ASpeedDD[i] = ASpeedDD[0];
      }
      DisplaySpeeds();
      break;

    case HelpC:
      Serial.println("Value....................Syntax");
      Serial.println("Set speed via");
      Serial.println("acceleration.............i <motor 2 - 4> <float>");
      Serial.println("deceleration.............d <motor 2 - 4> <float>");
      Serial.println("normal...................s <motor 1 - 4> <float>");
      Serial.println();
      Serial.println("set flags via");
      Serial.println("frequency control........fc <+,->");
      Serial.println("phase control............pc <+,->");
      Serial.println("enables potiinput........po <+,->");
      Serial.println("AutoMode.................au <+, ->");
      Serial.println("If no or wrong argument was given, default \"+\" will be used.");
      Serial.println();
      Serial.println("other");
      Serial.println("display RotorCounts......c");
      Serial.println("Set all to Master speed..a");
      Serial.println("This Info................help");
      break;

    case AutoC:
      AutoMode = (bool)value1;

      if (!AutoMode)
        AutoState = 0;

      Serial.println("AutoMode = " + (String)AutoMode);
      break;
  }
}


void DisplaySpeeds() {// display and assign speeds

  // fill the slave speeds memory with normal numbers
  for (byte i = 0; i < 3; i++) // rotor number - 1
    for (byte j = 0; j < 3; j++)// index
      SSpeed[i][j] = ASpeed[i + 1], SSpeedDD[i][j] = ASpeedDD[i + 1];

  // add speeds for acceleration
  for (byte i = 0; i < 3; i++) {// rotor number - 1
    SpeedAdder(&SSpeed[i][2], &SSpeedDD[i][2], SSpeed[i][1] - 10);
  }

  // substract speeds for deceleration
  for (byte i = 0; i < 3; i++) // rotor number - 1
    SpeedAdder(&SSpeed[i][0], &SSpeedDD[i][0], (int)(-SSpeed[i][1] / 3) );

  // print out speed memory
  /*for (byte i = 0; i < 3; i++) // rotor number - 1
    for (byte j = 0; j < 3; j++)// index
    Serial.print(SSpeed[i][j]), Serial.print("."), Serial.println(SSpeedDD[i][j]);*/

  for (byte j = 0; j < 4; j++) {
    Serial.print("M" + (String)(j + 1) + " =  " + (String)ASpeed[j] + "."  );
    if (ASpeedDD[j] < 10)Serial.print("0");
    Serial.print((String)ASpeedDD[j] + "   ");
  }
  Serial.println();
}

//!Todo: stehenbleiben: not finished!
void GetRotorCounts() {
  FCTimer = millis();
  for (int i = 0; i < 4; i++)
    GotPeriod[i] = 0, LastState[i] = digitalRead(ReadPins[i]);
  CountFinished = false;

  // GotPeriod is 0 at the beginning, is 1, when the startramp has been detected, is 2, when the next ramp is detected, and is 3 for the overnext ramp
  while (!CountFinished) {
    if (millis() - FCTimer > 10000) break;

    for (int j = 0; j < 4; j++) {
      StateNow[j] = digitalRead(ReadPins[j]);
      if (LastState[j] != StateNow[j]) {
        GotPeriod[j]++;
        if (GotPeriod[j] == 1 )RotorCount[j] = micros();
        else if (GotPeriod[j] == 3)RotorCount[j] = micros() - RotorCount[j];
      }
      LastState[j] = StateNow[j];
      //Serial.println();


      //when all arrived stop counting
      if (GotPeriod[0] >= 3 && GotPeriod[1] >= 3 && GotPeriod[2] >= 3 && GotPeriod[3] >= 3)
        CountFinished = true;
    }
  }

  //Serial.println((String)GotPeriod[0] + "   " + (String)GotPeriod[1] + "   " + (String)GotPeriod[2] + "   " + (String)GotPeriod[3] + "   ");
  Serial.println("M1C = " + (String)RotorCount[0] + "   M2C = " + (String)RotorCount[1] + "   M3C = " + (String)RotorCount[2] + "   M4C = " + (String)RotorCount[3]);
}

//does what the name says: sets the speed to float value
void SpeedSetter (byte *Speed, byte *SpeedDD, float NewSpeed) {

  *Speed = (byte)NewSpeed;
  *SpeedDD = round((NewSpeed - *Speed) * 100);
}

void SpeedAdder(byte *Speed, byte *SpeedDD, float adder) {
  float NewSpeed = *Speed + *SpeedDD / 100.0;

  NewSpeed += adder;

  NewSpeed = constrain(NewSpeed, 0, 255);

  *Speed = (byte)NewSpeed;
  *SpeedDD = round((NewSpeed - *Speed) * 100);
}
