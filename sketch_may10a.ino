const int powerPin = 25;
const int switchPin = 26;
const int ignitorPin = 27;

const int ignitorDelay = 30 * 1000; // msec
const int stabilizationDelay = 30 * 1000; // msec

class Relay {
  private:
  int pin;

  public:
  Relay(int _pin){
    pin = _pin;
    pinMode(pin, OUTPUT);
  }

  void turnOn(){
    digitalWrite(pin, HIGH);
  }

  void turnOff(){
    digitalWrite(pin, LOW);
  }
};

class Motor : Relay {
  public:
  Motor(int _pin) : Relay (_pin) {}

  void lowSpeed(){
    turnOn();
  }

  void HighSpeed(){
    turnOff();
  }
};

class Heater {
  public:
  Relay *power;
  Motor *motor;
  Relay *ignitor;

  public:
  Heater(Relay *_power, Motor *_motor, Relay *_ignitor){
    power = _power;
    motor = _motor;
    ignitor = _ignitor;
  }
  
  void run(){
    power->turnOn();
    motor->HighSpeed();
    delay(ignitorDelay);

    ignitor->turnOn();
    digitalWrite(2, HIGH);

    delay(stabilizationDelay);

    ignitor->turnOff();
    digitalWrite(2, LOW);

    delay(30 * 1000);

    // motor->HighSpeed();
  }
};

void setup() {
}

void loop() {
  // put your main code here, to run repeatedly:
    // put your setup code here, to run once:
  pinMode(2, OUTPUT);

  Relay *power = new Relay(powerPin);
  Motor *motor = new Motor(switchPin);
  Relay *ignitor = new Relay(ignitorPin);

  Heater ov65(power, motor, ignitor);

  delay(1000 * 10);
  // digitalWrite(2, HIGH);
  ov65.run();

  // ov65.ignitor->turnOn();

  while(true){

  }
}
