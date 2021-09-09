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

const byte WritePins[] = {10, 9, 5, 6};
const byte ReadPins[]  = {2, 3, 8, 12};

bool Found[] = {false, false, false, false};

byte LastSpeed = 15;

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

    delay(500);

  }
}

// the loop function runs over and over again forever
void loop() {
  byte mspeed;//motor speed
  bool lock = digitalRead(11);
  //digitalRead(12)&&digitalRead(8)&&digitalRead(3)&&digitalRead(2)
  if (lock) {
    digitalWrite(LED_BUILTIN, HIGH);

    if (Found[0] && Found[1] && Found[2] && Found[3]) {

      for (byte i = 0; i < 4; i++) {
        Found[i] = false;
      }
    } else {

      for (byte i = 0; i < 4; i++)
        Found[i] = Found[i] || digitalRead(ReadPins[i]);
    }


  }
  else {
    digitalWrite(LED_BUILTIN, LOW);   // turn the LED on (HIGH is the voltage level)

    if (!(Found[0] && Found[1] && Found[2] && Found[3]))
      for (byte i = 0; i < 4; i++) {
        Found[i] = false;
      }

  }

  mspeed = GetSpeed();

  for (byte i = 0; i < 4; i++) {

    //Serial.println(Found[i]);

    if (Found[i]) {
      analogWrite(WritePins[i], round(mspeed * 0.9));
      Serial.println("Got " + ((String)i) + " down!");
    }
    else
    {
      analogWrite(WritePins[i], mspeed);
      Serial.println("Got " + ((String)i) + " up!");
    }
  }
}

//

byte GetSpeed () {
  word PotNow = analogRead(A0);
  byte mspeed = (int)(sq((float)PotNow) * 0.000216026 + 0.02 * (float)PotNow + 8.0);

  if (mspeed < 15) mspeed = 0;

  mspeed = mspeed * 0.7 + LastSpeed * 0.3;

  LastSpeed = mspeed;

  //Serial.println("Speed now: " + ((String)mspeed));

  return mspeed;
}
