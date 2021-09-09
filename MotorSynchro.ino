/*

  M1 via Pin10 Transistor halb rechts oben
  M2 via Pin09 Transistor rechts oben
  M3 via Pin05 Transistor links oben
  M1 via Pin06 Transistor halb links oben

  RK1 in an LM324 Pin02 via LM324 Pin01 an Arduino Pin02
  RK2 in an LM324 Pin06 via LM324 Pin07 an Arduino Pin03
  RK3 in an LM324 Pin09 via LM324 Pin08 an Arduino Pin08
  RK4 in an LM324 Pin13 via LM324 Pin14 an Arduino Pin12

  Poti A0

  switch lock/unlock Pin11 lock-HIGH, unlock-LOW

*/

//arrays of arduino pins
const byte ReadPins[]  = {2, 3, 8, 12};
const byte WritePins[] = {10, 9, 5, 6};

bool Statesnow[4] = {false, false, false, false}; //array of states in every loop
word RotorCountNow[4] = {0, 0, 0, 0};
word MasterCount = 0;
bool StopCount[4] = {true, true, true, true};

int LastSpeed = 15;
byte Speed[4] = {0, 0, 0, 0};
bool Ready[3] = {false, false, false};

String OutStr[3] = {"", "", ""};
byte VisioCount = 0;

bool MasterState;

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
  if (GetSpeed () > 14) {
    Serial.println("Starting.");

    analogWrite(10, 65);
    analogWrite(9, 65);
    analogWrite(6, 65);
    analogWrite(5, 65);

    delay(200);

    byte mspeed = GetSpeed ();

    for (byte i = 0; i < 4 ; i++) {
      Speed[i] = mspeed;
      analogWrite(WritePins[i], mspeed);
    }
  }
}

// the loop function runs over and over again forever
void loop() {
  byte mspeed;//motor speed
  bool lock = digitalRead(11);
  if (lock) {
    digitalWrite(LED_BUILTIN, HIGH);

    for (byte i = 0; i < 4; i++) {
      Statesnow[i] = digitalRead(ReadPins[i]);

      if (!(i > 0) && (Statesnow[0] != MasterState)) {
        MasterState = Statesnow[i];

        if (!MasterState) {
          for (byte j = 0; j < 3; j++) {

            Serial.print(j + 2);
            Serial.print (" :  ");
            Serial.println(OutStr[j]);

            OutStr[j] = "";
          }

          for (byte j = 0; j < 4; j++) {

            if (j > 0) {
              analogWrite(WritePins[j], Speed[j]);
            }
            else
              analogWrite(WritePins[j], Speed[j]);

            Serial.print(" ");
            Serial.print(j + 1);
            Serial.print(": ");
            Serial.print(Speed[j]);
          }
          Serial.println();
          Serial.println();
        }

      } else 
      if ((i > 0) /*&& !(VisioCount)*/ && (MasterState) ) {
        if (Statesnow[i] != MasterState) {
          OutStr[i - 1] += "-";

          if (Ready[i - 1]) {
            if (abs(MasterCount - RotorCountNow[i] ) / MasterCount < 0.05 )
              analogWrite(WritePins[i], 0 );
            Ready[i - 1] = false;
          }
        } else {
          OutStr[i - 1] += "+";
          Ready[i - 1] = true;
        }
      }
    }

    VisioCount++;
    VisioCount %= 40;

    for (byte i  = 0; i < 4; i++) {

      if ((Statesnow[i] && StopCount[i])|| (RotorCountNow[i] > 14000)) {
        StopCount[i] = false;

        if (i > 0) {
          //Serial.println(RotorCountNow[i]);
          
          if (MasterCount > RotorCountNow[i])
            Speed[i] = constrain(Speed[i] - 1, 15, 254);
          else if (MasterCount < RotorCountNow[i])
            Speed[i] = constrain(Speed[i] + 1, 15, 254);
        }
        else {

          MasterCount = RotorCountNow[i];
          Speed[i] = GetSpeed ();
        }
        RotorCountNow[i] = 0;
      } else if (!Statesnow[i])
        StopCount[i] = true;

      RotorCountNow[i]++;
    }
  }
  else digitalWrite(LED_BUILTIN, LOW);   // turn the LED on (HIGH is the voltage level)

}

int GetSpeed () {
  int PotNow = analogRead(A0);
  byte mspeed = constrain(round (1 / (-0.02777840522 * log(PotNow) + 0.1955972043)), 0, 255);

  if (mspeed < 15) mspeed = 0;

  mspeed = mspeed * 0.5 + LastSpeed * 0.5;
  LastSpeed = mspeed;
  return mspeed;
}
