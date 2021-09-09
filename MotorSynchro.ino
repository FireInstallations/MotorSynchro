/*

  M1 via Pin10 Transistor halb rechts oben
  M2 via Pin09 Transistor rechts oben
  M3 via Pin05 Transistor links oben
  M4 via Pin06 Transistor halb links oben

  RK1 in an LM324 Pin02 via LM324 Pin01 an Arduino Pin02
  RK2 in an LM324 Pin06 via LM324 Pin07 an Arduino Pin03
  RK3 in an LM324 Pin09 via LM324 Pin08 an Arduino Pin08
  RK4 in an LM324 Pin13 via LM324 Pin14 an Arduino Pin12

  Poti A0

  switch lock/unlock Pin11 lock-HIGH, unlock-LOW
*/
/*Todo
  - Gegenphase
*/

const byte WritePins[] = {10, 9, 5, 6};
const byte ReadPins[]  = {2, 3, 8, 12};

byte LastSpeed = 15;
byte masterCounts = 1;

bool LastState[4] = {0, 0, 0, 0};
byte CountStates[4] = {0, 0, 0, 0};
byte Speed[4] = {0, 0, 0, 0};
byte FirstState[4] = {2, 2, 2, 2};

// the setup function runs once when you press reset or power the board
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

  byte mspeed = GetSpeed ();
  if (mspeed > 14) {
    //Serial.println("Starting.");

    analogWrite(10, 65);
    analogWrite(9, 65);
    analogWrite(6, 65);
    analogWrite(5, 65);

    delay(100);

    for (byte i = 0; i < 4; i++) {
      LastState[i] = digitalRead(ReadPins[i]);

      Speed[i] = mspeed;
    }
  }
}

// the loop function runs over and over again forever
void loop() {
  byte mspeed;//motor speed

  bool lock = digitalRead(11);
  if (lock) {
    digitalWrite(LED_BUILTIN, HIGH);

    if ((CountStates[0] < masterCounts) ) {

      for (byte i = 0; i < 4; i++) {
        bool StateNow = digitalRead(ReadPins[i]);
/*
        if (!VisioCount && !LastState[0]) {
          if (i)
            if (StateNow != LastState[0])
              Vision[i - 1] += ".";
            else
              Vision[i - 1] += "|";
        }*/

        if (LastState[i] != StateNow) {
          CountStates[i]++;
          /*Serial.print("i: ");
            Serial.print(i);
            Serial.print(": ");
            Serial.println(CountStates[i]);*/

          if (FirstState[i] == 2)
            FirstState[i] = StateNow;
          LastState[i] = StateNow;

          //Serial.println(((String)i) + " Nw St: " + ((String) StateNow));
        }
      }
      //Serial.println();

    } else {
      //int Maximum = 0;

      for (int i = 3; i >= 0; i--) {
        if (i > 0) {

          int Diff = CountStates[0] - CountStates[i];

          //Maximum = max(Maximum, abs(Diff));

          if (abs(Diff) < 5)
            Diff = sgn(Diff);

          if ((FirstState[i] != FirstState[0]) && !Diff) {
            //Diff = 3;

            /* Serial.print("Gegenphase Motor ");
              Serial.print(i + 1);
              Serial.println("!");*/
          }

          Diff = constrain(Speed[i] + Diff, 15, 254);

          if (Diff != Speed[i]) {
            /*Serial.print("NewSpeed for ");
              Serial.print(i + 1);
              Serial.print(": ");
              Serial.println(Diff);*/

            Speed[i] = Diff;
          }
        }

        FirstState[i] = 2;
        CountStates[i] = 0;
      }

      /*if (Maximum < 1) {
        //Serial.println("akurat++");
        masterCounts++;

        if (masterCounts >= 17)
          masterCounts = 1;

        }
        else if ((Maximum > 2) && (masterCounts > 1))
        masterCounts--;

        if (Maximum < 1 || Maximum > 2) {
        //Serial.print("New MasterCount = ");
        //  Serial.println(masterCounts);

        }*/


     /* if (!(Vision[0] == "" || Vision[1] == "" || Vision[2] == "")) {
        for (byte i = 0; i < 3; i++) {

          Serial.print(i + 2);
          Serial.print (" :  ");
          Serial.println(Vision[i]);
          Vision[i] = "";
        }

        Serial.println();
        Serial.println();
      }*/
    }
  }
  else {
    digitalWrite(LED_BUILTIN, LOW);   // turn the LED on (HIGH is the voltage level)

    for (byte i = 0; i < 4; i++) {
      CountStates[i] = 0;
      CountStates[i + 4] = 0;

      Speed[i] = Speed[0];
    }
  }
  mspeed = GetSpeed();

  if (mspeed > 0) {
    Speed[0] = mspeed;
  }

  for (byte i = 0; i < 4; i++) {
    analogWrite(WritePins[i], Speed[i]);
  }
}

//signum
template <typename T> int sgn(T val) {
  return (T(0) < val) - (val < T(0));
}

//get
byte GetSpeed () {
  int PotNow = analogRead(A0);
  byte mspeed = constrain(round (1 / (-0.02777840522 * log(PotNow) + 0.1955972043)), 0, 255);

  if (mspeed < 15) mspeed = 0;

  mspeed = mspeed * 0.5 + LastSpeed * 0.5;
  LastSpeed = mspeed;

  /*Serial.print("Speed now: ");
    Serial.print(mspeed);
    Serial.print(" bei ");
    Serial.println(PotNow);*/

  return mspeed;
}
