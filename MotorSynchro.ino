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

// States in a single main loop
bool Statesnow[4] = {false, false, false, false};
// individual speed for every rotor
byte Speed[4] = {0, 0, 0, 0};
// phase and frequency regulation are separated.
// So we handle phase only if every rotor have gotten a new speed;
byte GotSpeed[4] = {0, 0, 0, 0};
//How many loops since last periodic reset
word RotorCountNow[4] = {0, 0, 0, 0};
//needed for speed calculation & phase stopping
word LastRotorCount[4] = {0, 0, 0, 0};
//have we already seen a low in frequency regulation
bool StopCount[4] = {true, true, true, true};
//flattening for GetSpeed()
int LastSpeed = 15;
// did we already seen a high before a low in phase regulation?
bool Ready[3] = {false, false, false};
//strings that countains phase regulation highs & lows
String LastSlaveStates[3] = {"", "", ""};
//Counts loop  % 40 for LastSlaveStates
byte SlaveStatesResolutionCount = 0;
//last masterstate for phase regulation
bool MasterState;
//wait for a low in master until we start phase regulation
bool StartPhaseReg = false;


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
  if (GetSpeed () > 14) {
    Serial.println("Starting.");

    analogWrite(10, 65);
    analogWrite(9, 65);
    analogWrite(6, 65);
    analogWrite(5, 65);

    delay(200);
    // and give all master speed
    byte mspeed = GetSpeed ();

    for (byte i = 0; i < 4 ; i++) {
      Speed[i] = 19; //mspeed;
      analogWrite(WritePins[i], mspeed);
    }
  }
}


void loop() {
  byte mspeed;//motor speed
  bool lock = digitalRead(11);
  if (lock) {
    digitalWrite(LED_BUILTIN, HIGH);

    //read all states from rotor and set speed
    for (byte i = 0; i < 4; i++)
      Statesnow[i] = digitalRead(ReadPins[i]);

    //phaseregulation
    if ((GotSpeed[0] > 1) && (GotSpeed[1] > 1)  && (GotSpeed[2] > 1) && (GotSpeed[3] > 1) ) {

      //get Masterspeed and set if it has changed
      /* byte NewMasterSpeed = 19;//GetSpeed();
        if ( Speed[0] != NewMasterSpeed) {
         analogWrite(WritePins[0], Speed[0]);
         Speed[0] = NewMasterSpeed;
        }*/

      //write down just every 40th loop
      SlaveStatesResolutionCount++;
      SlaveStatesResolutionCount %= 90;

      // did we get a new Masterstate?
      if (Statesnow[0] != MasterState) {
        // save it
        MasterState = Statesnow[0];

        // if we got from white to black on master
        if (!MasterState && StartPhaseReg) {

          //set speed for every rotor (starts stopped for phase regulation ones again)
          for (byte i = 0; i < 4; i++) {
            analogWrite(WritePins[i], Speed[i]);

            //prepare frequency regulation
            GotSpeed[i] = false;
            RotorCountNow[i] = 0;
            StopCount[i] = false;

            //print every speed
            Serial.print(" ");
            Serial.print(i + 1);
            Serial.print(": ");
            Serial.print(Speed[i]);
          }
          Serial.println();
          Serial.println();

          //print Our  slaveStatesString and clear them
          for (byte i = 0; i < 3; i++) {
            Serial.print(i + 2);
            Serial.print (" :  ");
            Serial.println(LastSlaveStates[i]);

            LastSlaveStates[i] = "";
          }


        } else if (!MasterState) {//start phase regulation
          StartPhaseReg = true;

          SlaveStatesResolutionCount = 0;

          //calculate its new Slave speed
          for (byte i = 1; i < 4; i++) {
            //Serial.println(LastRotorCount[i]);

            if (LastRotorCount[0] > LastRotorCount[i])
              Speed[i] = constrain(Speed[i] - 1, 15, 254);
            else if (LastRotorCount[0] < LastRotorCount[i])
              Speed[i] = constrain(Speed[i] + 1, 15, 254);
          }

        }
      }
      //if master is on white (and the right resolution was reached)
      //add a "+" for slave high and "-" for slave low
      if (StartPhaseReg && MasterState && !(SlaveStatesResolutionCount)) {
        for (byte i = 0; i < 3; i ++) { //slaves

          // if master and slave are both white
          if (Statesnow[i + 1] == MasterState) {
            LastSlaveStates[i] += "+";


            //first high since last low
            if (LastSlaveStates[i][0] == '-')    {
              //accelerate rotor to move phase if frequency is less then 15% false
              if (( abs((long)LastRotorCount[0] - (long)LastRotorCount[i + 1])  / (float)LastRotorCount[0]  ) < 0.15 )
                analogWrite(WritePins[i + 1], constrain(round(Speed[i + 1] * 1.5), 15, 255) );
            } else
              analogWrite(WritePins[i + 1], Speed[i + 1]);

          } else { //master white slave black
            LastSlaveStates[i] += "-";

            //first low since last high
            if (LastSlaveStates[i][0] == '+') {

              //stop rotor to move phase if frequency is less then 15% false
              if (( abs((long)LastRotorCount[0] - (long)LastRotorCount[i + 1])  / (float)LastRotorCount[0]  ) < 0.15 ) {
                analogWrite(WritePins[i + 1], constrain(round(Speed[i + 1] * 0.6), 15, 255) );
              }
            } else
              analogWrite(WritePins[i + 1], Speed[i + 1]);
          }
        }
      }
    } else {//frequency regulation
      for (byte i  = 0; i < 4; i++) {
        // if one slave has stopped add 40 to speed once
        if ((RotorCountNow[i] > 19000) && (GotSpeed[i] < 3) ) {
          if (i > 0)
            Speed[i] = constrain(Speed[i] + 40, 15, 254);
          else
            Speed[0] = GetSpeed();

          RotorCountNow[i] = 1;

          GotSpeed[i] = 2;

        } else

          //did we found a high after we found a low?
          if (Statesnow[i] && StopCount[i]) {
            StopCount[i] = false;
            LastRotorCount[i] = RotorCountNow[i];

            // if all rotors got speed we are done
            GotSpeed[i] = constrain(GotSpeed[i] + 1, 0, 255);
            RotorCountNow[i] = 0;

            //prepare phase regulation
            StartPhaseReg = false;
            SlaveStatesResolutionCount = 0;

          } else if (!Statesnow[i])
            StopCount[i] = true;

        //count how many loops for one period
        RotorCountNow[i]++;

        /*
          Serial.print(i + 1);
          Serial.print(" RCN: ");
          Serial.print(RotorCountNow[i]);
          Serial.print(" GS: ");
          Serial.println(GotSpeed[i]);*/
      }

    }
  }
  else
    digitalWrite(LED_BUILTIN, LOW);   // turn the LED on (HIGH is the voltage level)

}

//get Speed form potentiometer
int GetSpeed () {
  int PotNow = analogRead(A0);
  byte mspeed = constrain(round (1 / (-0.02777840522 * log(PotNow) + 0.1955972043)), 0, 255);

  if (mspeed < 15) mspeed = 0;

  mspeed = mspeed * 0.5 + LastSpeed * 0.5;
  LastSpeed = mspeed;
  return mspeed;
}
