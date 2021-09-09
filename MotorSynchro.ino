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
bool StateNow[4] = {false, false, false, false};
// individual speed for every rotor
byte Speed[4] = {0, 0, 0, 0};
// orginal values +1
byte ShadowSpeed[4] = {0, 0, 0, 0};
// fine sloave speed control
byte SpeedDecPlace[4] = {0, 0, 0, 0};
// find half periode in frequency regulation, we have to find at least 3, so a value of 2 means this rotor got all needed counts.
byte GotHalfPeriod[4] = {0, 0, 0, 0};
//How many loops since last periodic reset
word RotorCount[4] = {0, 0, 0, 0};
//flattening for GetPotiSpeed()
float LastSpeed = 20;
// did we already seen a high before a low in phase regulation?
bool Ready[3] = {false, false, false};
//strings that countains phase regulation highs & lows
String LastSlaveStates[3] = {"", "", ""};
//Counts loop  % 40 for LastSlaveStates
byte LoopCount = 0;
//last Rotorstates
bool LastState[4];
//wait for a low in master until we start phase regulation
bool StartPhaseReg = false;
word lastMastercount = 0;

bool SlaveStop[3] = {false, false, false};

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
  //if (GetPotiSpeed () > 14) {
  Serial.println("Starting.");

  analogWrite(10, 65);
  analogWrite(9, 65);
  analogWrite(6, 65);
  analogWrite(5, 65);

  delay(200);

  byte mspeed = analogRead(A0) * 50.0 / 1023.0;
  LastSpeed = mspeed;
  // and give all master speed

  GetPotiSpeed ();
  for (byte i = 0; i < 4 ; i++) {
    Speed[i] = mspeed; //mspeed;
    ShadowSpeed[i] = Speed[i] + 1;
    analogWrite(WritePins[i], Speed[i]);
  }
  delay(200);
}


void loop() {
  bool lock = digitalRead(11);
  if (lock) {
    digitalWrite(LED_BUILTIN, HIGH);

    //write down just every 100th loop
    LoopCount++;
    LoopCount %= 100;

    for (byte i = 0; i < 4; i++) {
      if (!i || !SlaveStop[i - 1]) {
        if (LoopCount == 0) {
          Speed[i] = ShadowSpeed[i];
          analogWrite(WritePins[i], Speed[i]);
        }

        if (LoopCount == SpeedDecPlace[i]) {
          Speed[i] = ShadowSpeed[i] - 1;
          analogWrite(WritePins[i], Speed[i]);
        }
        /*Serial.print(" i: ");
          Serial.print(i);
          Serial.print(" LC: ");
          Serial.print(LoopCount);
          Serial.print(" DP: ");
          Serial.print(SpeedDecPlace[i]);
          Serial.print(" Sp: ");
          Serial.print(Speed[i]);*/
      }
    }
    //Serial.println();

    //read the states from all rotors and set speed
    for (byte i = 0; i < 4; i++)
      StateNow[i] = digitalRead(ReadPins[i]);

    //phaseregulation if all rotors have got their new speeds
    if ((GotHalfPeriod[0] > 3) && (GotHalfPeriod[1] > 3)  && (GotHalfPeriod[2] > 3) && (GotHalfPeriod[3] > 3) && !(GotHalfPeriod[0] == 255) ) {

      // did we get a new Masterstate?
      if (StateNow[0] != LastState[0]) {

        // if we got from white to black on master
        if (!LastState[0] && StartPhaseReg) {

          //set speed for every rotor (starts stopped for phase regulation ones again)
          for (byte i = 0; i < 4; i++) {
            Serial.print(" ");
            Serial.print(i + 1);
            Serial.print(" RC: ");
            Serial.print(RotorCount[i]);
            Serial.print(" ");
            //print every speed
            Serial.print("Sp: ");
            Serial.print(Speed[i]);

            //prepare frequency regulation
            if ( (( abs((long)RotorCount[0] - (long)RotorCount[i])  / (float)RotorCount[0]  ) < 0.02) ) {
              if (i)
                Serial.print(" Stabiel");
              else {
                if (!((( abs((long)RotorCount[0] - (long)RotorCount[1])  / (float)RotorCount[0]  ) < 0.02) && (( abs((long)RotorCount[0] - (long)RotorCount[2])  / (float)RotorCount[0]  ) < 0.02) && (( abs((long)RotorCount[0] - (long)RotorCount[3])  / (float)RotorCount[0]  ) < 0.02))) {
                  RotorCount[i] = 0;
                  GotHalfPeriod[i] = 0;
                } else
                  Serial.print(" Stabiel");
              }
            } else {
              RotorCount[i] = 0;
              GotHalfPeriod[i] = 0;
            }

            LastState[i] = StateNow[i];
          }
          Serial.println();
          Serial.println();

          //print Our  slaveStatesString and clear them
          for (byte i = 0; i < 3; i++) {
            //restart slaves
            SlaveStop[i] = false;

            //print last states
            Serial.print(i + 2);
            Serial.print (" :  ");
            Serial.println(LastSlaveStates[i]);

            LastSlaveStates[i] = "";
          }

        } else if (!LastState[0]) {//start phase regulation
          StartPhaseReg = true;

          for (byte i = 1; i < 4; i++) {
            float NewSpeed = ShadowSpeed[i] - 1 + SpeedDecPlace[i] / 100.0;
            if (RotorCount[i] > RotorCount[0])
              NewSpeed += 0.2;
            else if (RotorCount[i] < RotorCount[0])
              NewSpeed -= 0.2;

            NewSpeed = constrain(NewSpeed, 15, 255);

            Speed[i] = (byte)NewSpeed;
            ShadowSpeed[i] = Speed[i] + 1;
            SpeedDecPlace[i] = round((NewSpeed - Speed[i]) * 100);
          }

        }

        // save Masterstate
        LastState[0] = StateNow[0];
      }
      //if master is on white (and the right resolution was reached)
      //add a "+" for slave high and "-" for slave low
      if (StartPhaseReg && LastState[0] && !(LoopCount)) {
        for (byte i = 0; i < 3; i ++) { //slaves

          // if master and slave are both white
          if (StateNow[i + 1] == LastState[0]) {
            LastSlaveStates[i] += "+";

            //first high since last low
            if ((LastSlaveStates[i][0] == '-') && !SlaveStop[i])    {
              //accelerate rotor to move phase if frequency is less then 15% false
              if (( abs((long)RotorCount[0] - (long)RotorCount[i + 1])  / (float)RotorCount[0]  ) < 0.15 ) {
                //analogWrite(WritePins[i + 1], constrain(round(Speed[i + 1] * 1.1), 15, 255));
                //analogWrite(WritePins[i + 1], constrain(round(Speed[i + 1] * 1.5), 15, 255) );
                analogWrite(WritePins[i + 1], ShadowSpeed[i + 1]  );
                SlaveStop[i] = true;
              }
            } else
              SlaveStop[i] = false;

          } else { //master white slave black
            LastSlaveStates[i] += "-";

            //first low since last high
            if ((LastSlaveStates[i][0] == '+')  && !SlaveStop[i]) {

              //stop rotor to move phase if frequency is less then 15% false
              if (( abs((long)RotorCount[0] - (long)RotorCount[i + 1])  / (float)RotorCount[0]  ) < 0.05 ) {
                //analogWrite(WritePins[i + 1], constrain(round(Speed[i + 1] * 0.9), 15, 255));
                //  analogWrite(WritePins[i + 1], constrain(round(Speed[i + 1] * 0.6), 15, 255) );
                analogWrite(WritePins[i + 1], ShadowSpeed[i + 1] -1 );
                SlaveStop[i] = true;
              }
            } else
              SlaveStop[i] = false;
          }
        }
      }
    } else {//period length measurement for frequency regulation
      for (byte i  = 0; i < 4; i++) {
        //Serial.println( RotorCount[0]);
        // if one slave has stopped add 40 to speed once
        if ((RotorCount[i] > 18000) && ((GotHalfPeriod[i] < 4) || (!i) )) {
          //Serial.println(i);

          if (i > 0) {
            Speed[i] = constrain(Speed[i] + 40, 15, 254);

            RotorCount[i] = RotorCount[0] + 1;
            GotHalfPeriod[i] = 5; //ignore next counts
          }
          else {

            GetPotiSpeed ();
            GetSerialSpeed();

            for (byte j = 0; j < 4; j++) {
              analogWrite(WritePins[j], Speed[0]);
            }

            /*if (Speed[0] < 65) {
              for (byte j = 0; j < 4; j++)
                GotHalfPeriod[j] = 255;
              //Serial.println("Master Stoped");
            }
            else {
              for (byte j = 0; j < 4; j++)
                GotHalfPeriod[j] = 5;
              Serial.println("Master Started");'/
            }
          }


        } else {
          // did we found a new half periood?
          if (StateNow[i] != LastState[i]) {
            GotHalfPeriod[i] = constrain(GotHalfPeriod[i] + 1, 0, 254);
            LastState[i] = StateNow[i];

            /*Serial.print(i);
              Serial.print(" StN: ");
              Serial.print(StateNow[i]);
              Serial.print(" GHP: ");

              Serial.print(GotHalfPeriod[i]);
              Serial.print(" RC: ");
              Serial.println(RotorCount[i]);*/

          }

          switch (GotHalfPeriod[i]) {
            case 0:
              RotorCount[i] = 0;
              break;
            case 1:
              RotorCount[i]++;
              break;
            case 2:
              RotorCount[i]++;
              break;
            case 3:
              //prepare phase regulation
              StartPhaseReg = false;
              LoopCount = 0;

              //ignore next counts
              GotHalfPeriod[i]++;

              //set new Slave speed according to frequency determination
              //Serial.println(RotorCount[i]);

              if (!i) {//if master
                GetPotiSpeed ();
                GetSerialSpeed ();
              }
              break;
          }
        }
      }
    }
  }
  else
    digitalWrite(LED_BUILTIN, LOW);   // turn the LED on (HIGH is the voltage level)

}

//get Speed from potentiometer
void GetPotiSpeed () {
  float NewSpeed = analogRead(A0) * 67.0 / 1023.0;
  byte maxRotor = 1;

  if ( abs(NewSpeed - LastSpeed) > 8) {
    maxRotor = 4;
    Serial.println(" Got new Speed!");
  }

  NewSpeed = NewSpeed * 0.05 + LastSpeed * 0.95;

  for (byte i = 0; i < maxRotor; i++)
    if (NewSpeed < 15) {
      Speed[i] = 0;
      SpeedDecPlace[i] = 0;
    } else {
      Speed[i] = (byte)NewSpeed;
      ShadowSpeed[i] = Speed[0] + 1;
      SpeedDecPlace[i] = round((NewSpeed - Speed[0]) * 100);
    }

  LastSpeed = NewSpeed;
}

void GetSerialSpeed () {
  if (Serial.available() > 0) {
    // read the incoming byte:
    float NewSpeed = (Serial.readString()).toFloat();
    NewSpeed = constrain(NewSpeed, 15, 254);

    Speed[0] = (byte)NewSpeed;
    ShadowSpeed[0] = Speed[0] + 1;
    SpeedDecPlace[0] = round((NewSpeed - Speed[0]) * 100);

    //start with 0 again so we count right
    LoopCount = 0;
  }
}

//Todo: FloatTobytespeed?
