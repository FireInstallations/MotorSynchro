/*
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

  switch lock/unlock Arduino Pin11 lock-HIGH, unlock-LOW

*/

//arrays of arduino pins
const byte ReadPins[]  = {2, 3, 8, 12};// reflex sensors 0 to 3
const byte WritePins[] = {10, 9, 5, 6};// motors 0 to 3

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
  Serial.begin(9600);

  // some time for startup
  delay(2000);

  //if they should rotate, start them once
  //if (GetPotiSSpeed () > 14) {
  Serial.println("Starting.");

  analogWrite(10, 65);
  analogWrite(9, 65);
  analogWrite(6, 65);
  analogWrite(5, 65);

  delay(200);

  LastSpeed = analogRead(A0) * 50.0 / 1023.0;

  // and give all master SSpeed
  for (byte i = 0; i < 4 ; i++) {
    ASpeed[i] = LastSpeed; //mSSpeed;
    analogWrite(WritePins[i], ASpeed[i]);
  }
  delay(200);
  FCTimer = millis();
}

void loop() {
  bool lock = digitalRead(11);
  if (lock) {
    digitalWrite(LED_BUILTIN, HIGH);

    //write down just every 100th loop
    LoopCount++;
    if (LoopCount == 100)LoopCount = 0;
    // realize intermediate values between SSpeed and ShadowSSpeed with loop counter
    for (byte i = 0; i < 4; i++) {

      if (LoopCount == 0)
        analogWrite(WritePins[i], ASpeed[i] + 1);
      if (LoopCount == ASpeedDD[i])
        analogWrite(WritePins[i], ASpeed[i]);

    }// end of for loop

    //Get serial
    GetSerial();
    //Get potiSSpeed if Potiflag
    if (PotiFlag)GetPotiSSpeed();// gets the potiSSpeed for master

    // SSpeed control

    if (millis() - FCTimer > 2000 && FCFlag && CountFinished) {
      FCTimer = millis();
      GetRotorCounts();

      for (byte i = 1; i < 4; i++) {
        if (RotorCount[i] > RotorCount[0])
          SpeedAdder(ASpeed[i], ASpeedDD[i], 0.2);

        else if (RotorCount[i] < RotorCount[0])
          SpeedAdder(ASpeed[i], ASpeedDD[i], -0.2);

      }
    }
    // finish condition of frequency control
    if (FCFlag) {
      if (( abs((long)RotorCount[0] - (long)RotorCount[1])  / (float)RotorCount[0]  ) < 0.05 &&
         ( abs((long)RotorCount[0] - (long)RotorCount[2])  / (float)RotorCount[0]  ) < 0.05 &&
         ( abs((long)RotorCount[0] - (long)RotorCount[3])  / (float)RotorCount[0]  ) < 0.05 ) {
            if (FCFlag)
              Serial.println("frequency locked");
            FCFlag = false;
          }
    }

    // store all states until the master is white
    if (!WPFlag)
      for (byte i = 0; i < 4; i++)
        LastState[i] = digitalRead(ReadPins[i]);

    if (DFlag && !LoopCount) {// if display is active, loop counter = 0
      StateNow[0] = digitalRead(ReadPins[0]);// look for a new Master state
      if (StateNow[0] != LastState[0]) {// Master has changed
        if (!LastState[0]) {
          WPFlag = true;// if the last Master state was black, it is white now
          LastState[0] = StateNow[0];// store the actual white Master state
        }
        else WPFlag = false;// if the last Master state was white, it is black now
      }
      if (WPFlag) { // it is the white period of Master

        for (byte i = 1; i < 4; i++) {
          // read slave stati
          StateNow[i] = digitalRead(ReadPins[i]);
          // write the slave strings
          if ( StateNow[i])
            SlaveString[i - 1] += "+"; else SlaveString[i - 1] += "-";

          if (PCFlag) {// if Phase Control is active
            // regulate the phases of the slaves

            if (LastState[i] == StateNow[i] && StateNow[i] == LOW) // accelerate as long as leading minusses occur
              ASpeed[i] = SSpeed[i - 1][2], ASpeedDD[i] = SSpeedDD[i - 1][2];
            else if (StateNow[i] == HIGH) // return to normal slave speed for the plusses following, this is the ideal situation
              ASpeed[i] = SSpeed[i - 1][1], ASpeedDD[i] = SSpeedDD[i - 1][1];
            if (LastState[i] == StateNow[i] && LastState[i] == HIGH) // normal speeds for plusses persisting
              ASpeed[i] = SSpeed[i - 1][1], ASpeedDD[i] = SSpeedDD[i - 1][1];
            else if (StateNow[i] == LOW)// decelerate if trailing minusses occur
              ASpeed[i] = SSpeed[i - 1][2], ASpeedDD[i] = SSpeedDD[i - 1][2];

          }
        }// end of for loop

      } else if (LastState[0]) {// in the next loop LastState[0] is updated and is low, so it is done only once 
        // the black period of Master has started, print out the slave string and clear it
        //return to normal slave speeds
        for (byte i = 1; i < 4; i++)
          ASpeed[i] = SSpeed[i - 1][1], ASpeedDD[i] = SSpeedDD[i - 1][1];
        // display results
        if (DFlag) {
          // print out the normal speeds of all rotors
          DisplaySpeeds();
          // print out the strings
          for (byte i = 0; i < 3; i++) {
            Serial.print(i + 2);
            Serial.print (" :  ");
            Serial.println(SlaveString[i]);

            SlaveString[i] = "";
          }
        }
      }
    }
  }
  else
    digitalWrite(LED_BUILTIN, LOW);   // turn the LED on (HIGH is the voltage level)

}

//***************************** subroutines ********************************************************************

void GetPotiSSpeed () {//get SSpeed for the master from potentiometer
  float NewSpeed = analogRead(A0) * 67.0 / 1023.0;


  if ( abs(NewSpeed - LastSpeed) > 3) {

    DisplaySpeeds();
  }

  NewSpeed = NewSpeed * 0.01 + LastSpeed * 0.99;


  if (NewSpeed < 15) {
    for (int i = 0; i < 4; i++)
      ASpeed[i] = 0, ASpeedDD[i] = 0;
  } else {
    ASpeed[0] = (byte)NewSpeed;
    ASpeedDD[0] = round((NewSpeed - ASpeed[0]) * 100);
  }

  LastSpeed = NewSpeed;
}

void GetSerial () {// commands over serial monitor
  // the serial monitor must have kein Zeilenende
  if (Serial.available() > 0) {
    // read the incoming String:
    int i = 0;
    String instring = Serial.readString();
    if (instring.startsWith("po") || instring.startsWith("Po"))
      PotiFlag = true/*,Serial.println("PotiFlag = " + (String)PotiFlag)*/;
    else {

      PotiFlag = false, Serial.println("PotiFlag = " + (String)PotiFlag);
      if (instring.startsWith("fc") || instring.startsWith("FC"))
        FCFlag = true, Serial.println("FCFlag = " + (String)FCFlag);
      else {
        FCFlag = false, Serial.println("FCFlag = " + (String)FCFlag);
        if (instring.startsWith("pc") || instring.startsWith("PC"))
          PCFlag = true, Serial.println("PCFlag = " + (String)PCFlag);
        else {
          PCFlag = false, Serial.println("PCFlag = " + (String)PCFlag);
          if (instring.startsWith("a") || instring.startsWith("A")) {
            for (byte i = 0; i < 4 ; i++) {
              ASpeed[i] = ASpeed[0]; //MasterSpeed
              ASpeedDD[i] = ASpeedDD[0];
            }
            DisplaySpeeds();
          }
          else if (instring.startsWith("c") || instring.startsWith("C"))

            GetRotorCounts();

          else {
            if (instring.endsWith("M1"))
              i = 0;
            if (instring.endsWith("M2"))
              i = 1;
            if (instring.endsWith("M3"))
              i = 2;
            if (instring.endsWith("M4"))
              i = 3;

            float NewSpeed = instring.toFloat();
            NewSpeed = constrain(NewSpeed, 15, 254);
            ASpeed[i] = (byte)NewSpeed;
            ASpeedDD[i] = round((NewSpeed - ASpeed[i]) * 100);
            DisplaySpeeds();

            //start with 0 again so we count right
            LoopCount = 0;
          }
        }
      }
    }
  }
}


void DisplaySpeeds() {// display and assign speeds
  
  // fill the slave speeds memory with normal numbers
  for (byte i = 0; i < 3; i++) // rotor number - 1
    for (byte j = 0; j < 3; j++)// index
      SSpeed[i][j] = ASpeed[i + 1], SSpeedDD[i][j] = ASpeedDD[i + 1];
      
  // add speeds for acceleration
  for (byte i = 0; i < 3; i++) // rotor number - 1
    SpeedAdder(SSpeed[i][2], SSpeedDD[i][2], 1);
    
  // substract speeds for deceleration
  for (byte i = 0; i < 3; i++) // rotor number - 1
    SpeedAdder(SSpeed[i][0], SSpeedDD[i][0], -1);
    
  // print out speed memory
  for (byte i = 0; i < 3; i++) // rotor number - 1
    for (byte j = 0; j < 3; j++)// index
      Serial.print(SSpeed[i][j]), Serial.print("."), Serial.println(SSpeedDD[i][j]);

  for (byte j = 0; j < 4; j++) {
    Serial.print("M" + (String)(j + 1) + " =  " + (String)ASpeed[j] + "."  );
    if (ASpeedDD[j] < 10)Serial.print("0");
    Serial.print((String)ASpeedDD[j] + "   ");
  }
  Serial.println();
}

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

void SpeedAdder(byte Speed, byte SpeedDD, float adder) {
  
  float NewSpeed = Speed + SpeedDD / 100.0;

  NewSpeed += adder;

  NewSpeed = constrain(NewSpeed, 15, 255);

  Speed = (byte)NewSpeed;
  SpeedDD = round((NewSpeed - Speed) * 100);
}
