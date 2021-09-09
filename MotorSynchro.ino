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
byte Speed[4] = {50, 50, 50, 50};
// orginal values +1
byte ShadowSpeed[4] = {51, 51, 51, 51};
// fine slave speed control
byte SpeedDecDigits[4] = {0, 0, 0, 0};
// find half periode in frequency regulation, we have to find at least 3, so a value of 2 means this rotor got all needed counts.
byte GotPeriod[4] = {0, 0, 0, 0};
//How many loops since last periodic reset
long RotorCount[4] = {0, 0, 0, 0};
//flattening for GetPotiSpeed()
float LastSpeed = 20;
// did we already seen a high before a low in phase regulation?
bool Ready[3] = {false, false, false};
//strings that countains phase regulation highs & lows
String LastSlaveString[3] = {"", "", ""};
//Counts loop  % 40 for LastSlaveString
byte LoopCount = 0;
//last Rotorstates
bool LastState[4];
//wait for a low in master until we start phase regulation
bool StartPhaseReg = false;
word lastMastercount = 0;
// ist true, during the slave has a changed speed in phase regulation
bool SlaveStop[3] = {false, false, false};
bool PotiFlag = false;// enables potiinput
bool CountFinished = false;// indicates, if counting is finished
long WhileTimer;

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

  LastSpeed = analogRead(A0) * 50.0 / 1023.0;

  // and give all master speed


  for (byte i = 0; i < 4 ; i++) {
    Speed[i] = LastSpeed; //mspeed;
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
    // realize intermediate values between Speed and ShadowSpeed with loop counter
    for (byte i = 0; i < 4; i++) {

      if (LoopCount == 0)
        analogWrite(WritePins[i], Speed[i] + 1);
      if (LoopCount == SpeedDecDigits[i])
        analogWrite(WritePins[i], Speed[i]);

    }// end of for loop

    //Get serial speed
    GetSerial();
    //Get potiSpeed if Potiflag
    if (PotiFlag)GetPotiSpeed();// gets the potispeed for master

    // speed regulation
    // get Period
    //GetRotorCounts();


    /* //phaseregulation if all rotors have got their new speeds
      if ((GotPeriod[0] > 3) && (GotPeriod[1] > 3)  && (GotPeriod[2] > 3) && (GotPeriod[3] > 3) && !(GotPeriod[0] == 255) ) {

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
                 Serial.print(" Stabil");
               else {
                 if (!((( abs((long)RotorCount[0] - (long)RotorCount[1])  / (float)RotorCount[0]  ) < 0.02) && (( abs((long)RotorCount[0] - (long)RotorCount[2])  / (float)RotorCount[0]  ) < 0.02) && (( abs((long)RotorCount[0] - (long)RotorCount[3])  / (float)RotorCount[0]  ) < 0.02))) {
                   RotorCount[i] = 0;
                   GotPeriod[i] = 0;
                 } else
                   Serial.print(" Stabil");
               }
             } else {
               RotorCount[i] = 0;
               GotPeriod[i] = 0;
             }


           }
           Serial.println();
           Serial.println();

           //print slaveStatesString and clear it
           for (byte i = 0; i < 3; i++) {
             //restart slaves
             SlaveStop[i] = false;

             //print Last slave string
             Serial.print(i + 2);
             Serial.print (" :  ");
             Serial.println(LastSlaveString[i]);

             LastSlaveString[i] = "";
           }

         } else if (!LastState[0]) {//start phase regulation, when the master is white
           StartPhaseReg = true;

           for (byte i = 1; i < 4; i++) {
             float NewSpeed = ShadowSpeed[i] - 1 + SpeedDecDigits[i] / 100.0;
             if (RotorCount[i] > RotorCount[0])
               NewSpeed += 0.2;
             else if (RotorCount[i] < RotorCount[0])
               NewSpeed -= 0.2;

             NewSpeed = constrain(NewSpeed, 15, 255);

             Speed[i] = (byte)NewSpeed;
             ShadowSpeed[i] = Speed[i] + 1;
             SpeedDecDigits[i] = round((NewSpeed - Speed[i]) * 100);
           }

         }


       }
       //if master is on white (and the right resolution was reached)
       //add a "+" for slave high and "-" for slave low
       if (StartPhaseReg && LastState[0] && !(LoopCount)) {
         for (byte i = 0; i < 3; i ++) { //slaves

           // if master and slave are both white
           if (StateNow[i + 1] == LastState[0]) {
             LastSlaveString[i] += "+";

             //first high since last low
             if ((LastSlaveString[i][0] == '-') && !SlaveStop[i])    {
               //accelerate rotor to move phase if frequency is less then 5% false
               if (( abs((long)RotorCount[0] - (long)RotorCount[i + 1])  / (float)RotorCount[0]  ) < 0.05 ) {
                 //analogWrite(WritePins[i + 1], constrain(round(Speed[i + 1] * 1.1), 15, 255));
                 //analogWrite(WritePins[i + 1], constrain(round(Speed[i + 1] * 1.5), 15, 255) );
                 analogWrite(WritePins[i + 1], ShadowSpeed[i + 1]);
                 SlaveStop[i] = true;
               }
             } else
               SlaveStop[i] = false;

           } else { //master white slave black
             LastSlaveString[i] += "-";

             //first low since last high
             if ((LastSlaveString[i][0] == '+')  && !SlaveStop[i]) {

               //stop rotor to move phase if frequency is less then 5% false
               if (( abs((long)RotorCount[0] - (long)RotorCount[i + 1])  / (float)RotorCount[0]  ) < 0.05 ) {
                 //analogWrite(WritePins[i + 1], constrain(round(Speed[i + 1] * 0.9), 15, 255));
                 //  analogWrite(WritePins[i + 1], constrain(round(Speed[i + 1] * 0.6), 15, 255) );
                 analogWrite(WritePins[i + 1], ShadowSpeed[i + 1] - 1 );
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
         if ((RotorCount[i] > 18000) && ((GotPeriod[i] < 4) || (!i) )) {
           //Serial.println(i);

           if (i > 0) {
             Speed[i] = constrain(Speed[i] + 40, 15, 254);

             RotorCount[i] = RotorCount[0] + 1;
             GotPeriod[i] = 5; //ignore next counts
           }
           else {

             GetPotiSpeed ();
             GetSerial();

             for (byte j = 0; j < 4; j++) {
               analogWrite(WritePins[j], Speed[0]);
             }

             if (Speed[0] < 65) {
               for (byte j = 0; j < 4; j++)
                 GotPeriod[j] = 255;
               Serial.println("Master Stoped");
             }
             else {
               for (byte j = 0; j < 4; j++)
                 GotPeriod[j] = 5;
               Serial.println("Master Started");
             }
           }


         } else {
           // did we find a new half periood?
           if (StateNow[i] != LastState[i]) {
             GotPeriod[i] = constrain(GotPeriod[i] + 1, 0, 254);
             LastState[i] = StateNow[i];

             /*Serial.print(i);
               Serial.print(" StN: ");
               Serial.print(StateNow[i]);
               Serial.print(" GHP: ");

               Serial.print(GotPeriod[i]);
               Serial.print(" RC: ");
               Serial.println(RotorCount[i]);*/

    /* }

      switch (GotPeriod[i]) {
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
         GotPeriod[i]++;

         //set new Slave speed according to frequency determination
         //Serial.println(RotorCount[i]);

         if (!i) {//if master
           GetPotiSpeed ();
           GetSerial ();
         }
         break;
      }
      }
      }
      }*/
  }
  else
    digitalWrite(LED_BUILTIN, LOW);   // turn the LED on (HIGH is the voltage level)

}

//get Speed for the master from potentiometer
void GetPotiSpeed () {
  float NewSpeed = analogRead(A0) * 67.0 / 1023.0;


  if ( abs(NewSpeed - LastSpeed) > 3) {

    DisplaySpeeds();
  }

  NewSpeed = NewSpeed * 0.01 + LastSpeed * 0.99;


  if (NewSpeed < 15) {
    for (int i = 0; i < 4; i++)
      Speed[i] = 0, SpeedDecDigits[i] = 0;
  } else {
    Speed[0] = (byte)NewSpeed;
    ShadowSpeed[0] = Speed[0] + 1;
    SpeedDecDigits[0] = round((NewSpeed - Speed[0]) * 100);
  }

  LastSpeed = NewSpeed;
}

void GetSerial () {
  // the serial monitor must have kein Zeilenende
  if (Serial.available() > 0) {
    // read the incoming String:
    int i = 0;
    String instring = Serial.readString();
    if (instring.startsWith("p") || instring.startsWith("P"))
      PotiFlag = true, Serial.println("PotiFlag = " + (String)PotiFlag);
    else {
      PotiFlag = false, Serial.println("PotiFlag = " + (String)PotiFlag);
      if (instring.startsWith("a") || instring.startsWith("A")) {
        for (byte i = 0; i < 4 ; i++) {
          Speed[i] = Speed[0]; //Masterspeed
          SpeedDecDigits[i] = SpeedDecDigits[0];
          ShadowSpeed[i] = Speed[i] + 1;
        }
        DisplaySpeeds();
      }
      else if (instring.startsWith("c") || instring.startsWith("C")) {

        GetRotorCounts();

      }



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
        Speed[i] = (byte)NewSpeed;
        ShadowSpeed[i] = Speed[i] + 1;
        SpeedDecDigits[i] = round((NewSpeed - Speed[i]) * 100);
        DisplaySpeeds();

        //start with 0 again so we count right
        LoopCount = 0;
      }
    }
  }
}

void DisplaySpeeds() {
  for (byte j = 0; j < 4; j++) {
    Serial.print("M" + (String)(j + 1) + " =  " + (String)Speed[j] + "."  );
    if (SpeedDecDigits[j] < 10)Serial.print("0");
    Serial.print((String)SpeedDecDigits[j] + "   ");
  }
  Serial.println();
}

void GetRotorCounts() {
  for (int i = 0; i < 4; i++)
    GotPeriod[i] = 0, LastState[i] = digitalRead(ReadPins[i]);
  CountFinished = false;

  // GotPeriod is 0 at the beginning, is 1, when the startramp has been detected, is 2, when the next ramp is detected, and is 3 for the overnext ramp
  while (!CountFinished) {

    for (int j = 0; j < 4; j++) {
      StateNow[j] = digitalRead(ReadPins[j]);
      if (LastState[j] != StateNow[j])GotPeriod[j]++, Serial.print("+");
      if (GotPeriod[j] == 1 )RotorCount[j] = micros();
      else if (GotPeriod[j] == 3)RotorCount[j] = micros() - RotorCount[j];
      LastState[j] = StateNow[j];
      //Serial.println();


      //when all arrived stop counting
      if (GotPeriod[0] >= 3 && GotPeriod[1] >= 3 && GotPeriod[2] >= 3 && GotPeriod[3] >= 3)
        CountFinished = true;
    }
  }

  Serial.println((String)GotPeriod[0] + "   " + (String)GotPeriod[1] + "   " + (String)GotPeriod[2] + "   " + (String)GotPeriod[3] + "   ");
  Serial.println("M1C = " + (String)RotorCount[0] + "   M2C = " + (String)RotorCount[1] + "   M3C = " + (String)RotorCount[2] + "   M4C = " + (String)RotorCount[3]);
}
