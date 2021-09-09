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

byte LastSpeed = 15;
byte Place[] = {0, 0, 0, 0};
byte Pins[] = {10, 9, 5, 6};
byte Speed[] = {20, 20, 20, 20};

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
  /*
    //if they should rotate, start them once
    if (GetSpeed () > 14) {
      Serial.println("Starting.");

      analogWrite(10, 65);
      analogWrite(9, 65);
      analogWrite(6, 65);
      analogWrite(5, 65);

      delay(100);
    }*/
}

// the loop function runs over and over again forever
void loop() {
  byte mspeed;//motor speed
  bool lock = digitalRead(11);
  //digitalRead(12)&&digitalRead(8)&&digitalRead(3)&&digitalRead(2)


  for (byte i = 0; i < 4; i++) {
    //analogWrite(Pins[i], Speed[i]);
    Serial.print (" | Pl: " + ((String)Place[i]) + " Sp: " + ((String)Speed[i]) );
  }
  Serial.println();

  if (lock) {
    digitalWrite(LED_BUILTIN, HIGH);

    if (GetLow() > 0)
    {
      if (GetHigh() > 1) {
        Serial.println("Next Speed");

        for (byte i = 1; i < 4; i++) {

          if (Place[i] > Place[0]) Speed[i] -= 1;
          if (Place[i] < Place[0]) Speed[i] += 1;

        }

      }

      Serial.println("Resett");
      Place[0] = 0;
      Place[1] = 0;
      Place[2] = 0;
      Place[3] = 0;

    }
    else {
      byte RankNow = GetHigh() + 1;

      if (digitalRead(12) && !Place[3])
        Place[3] = RankNow;

      if (digitalRead(8) && !Place[2])
        Place[2] = RankNow;

      if (digitalRead(3) && !Place[1])
        Place[1] = RankNow;

      if (digitalRead(2) && !Place[0])
        Place[0] = RankNow;
    }

  }
  else {
    digitalWrite(LED_BUILTIN, LOW);   // turn the LED on (HIGH is the voltage level)

    Place[0] = 0;
    Place[1] = 0;
    Place[2] = 0;
    Place[3] = 0;

    for (byte i = 0; i < 4; i++)
      Speed[i] = GetSpeed ();

    Serial.println(GetSpeed ());
  }



  //Serial.println(mspeed);
}

byte GetSpeed () {
  int PotNow = analogRead(A0);
  byte mspeed = (int)(sq((float)PotNow) * 0.000216026 + 0.02 * (float)PotNow + 8.0);

  if (mspeed < 15) mspeed = 0;

  mspeed = mspeed * 0.7 + LastSpeed * 0.3;
  LastSpeed = mspeed;

  return mspeed;
}

byte GetHigh () {
  return max (Place[0], max (Place[1], max(Place[2], Place[3])));
}
byte GetLow () {
  return min (Place[0], min (Place[1], min(Place[2], Place[3])));
}
