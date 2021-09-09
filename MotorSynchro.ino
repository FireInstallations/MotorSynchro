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

int LastSpeed = 15;

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

    delay(100);
  }
}

// the loop function runs over and over again forever
void loop() {
  int mspeed;//motor speed
  bool lock = digitalRead(11);
  //digitalRead(12)&&digitalRead(8)&&digitalRead(3)&&digitalRead(2)
  if (lock)digitalWrite(LED_BUILTIN, HIGH);
  else digitalWrite(LED_BUILTIN, LOW);   // turn the LED on (HIGH is the voltage level)

  mspeed = GetSpeed ();

  analogWrite(10, mspeed);
  analogWrite(9, mspeed);
  analogWrite(6, mspeed);
  analogWrite(5, mspeed);
  Serial.println(mspeed);

}

int GetSpeed () {
  int PotNow = analogRead(A0);
  int mspeed = (int)(sq((float)PotNow) * 0.000216026 + 0.02 * (float)PotNow + 8.0);

  if (mspeed < 15) mspeed = 0;

  mspeed = mspeed * 0.7 + LastSpeed * 0.3;
  LastSpeed = mspeed;

  return mspeed;
}
