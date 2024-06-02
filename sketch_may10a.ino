#include <WiFi.h>
#include <WebServer.h>

// WiFi setting
const char* ssid = "OV-65";
const char* passworg = "01234567";

// IP setting
IPAddress local_ip(192,168,2,1);
IPAddress gateway(192,168,2,1);
IPAddress subnet(255,255,255,0);
WebServer server(80);

const int powerPin = 25;
const int switchPin = 26;
const int ignitorPin = 27;
const int valvePin = 28;

const int ignitorDelay = 30 * 1000; // msec
const int stabilizationDelay = 30 * 1000; // msec

bool heaterIsTurnOn = 0;

class Heater;
Heater *ov65;

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
  private:
  Relay *power;
  Motor *motor;
  Relay *ignitor;
  Relay *valve;
  // 0 = off 
  // 1 = start
  // 2 = on
  // 3 = stop
  // 4 = fail
  int state;

  public:
  Heater(Relay *_power, Motor *_motor, Relay *_ignitor, Relay *_valve){
    power = _power;
    motor = _motor;
    ignitor = _ignitor;
    valve = _valve;

    state = 0;
    
    power->turnOff();
    ignitor->turnOff();
    valve->turnOff();
  }

  int getState(){
    return state;
  }
  
  void run(){
    state = 2;
    power->turnOn();
    valve->turnOn();
    motor->HighSpeed();
    // delay(ignitorDelay);
    delay(5000);

    ignitor->turnOn();
    digitalWrite(2, HIGH);

    // delay(stabilizationDelay);

    ignitor->turnOff();
    digitalWrite(2, LOW);
    state = 2;
  }

  static void runImpl(void* _this){
    ((Heater*)_this)->run();
  }

  void heaterRun(){
    xTaskCreatePinnedToCore(this->runImpl, "ov65-run", 10000, this, 0, NULL, 1);
  }

  void stop(){
    state = 3;
    valve->turnOff();
    // 5 minutes
    // delay(5 * 60 * 1000);
    power->turnOff();
    state = 4;
  }

  static void stopImpl(void* _this){
    ((Heater*)_this)->stop();
  }

  void heaterStop(){
    xTaskCreatePinnedToCore(this->stopImpl, "ov65-stop", 10000, this, 0, NULL, 1);
  }
};

void handle_OnConnect() {
  server.send(200, "text/html", SendHTML()); 
}

void handle_heaterTunrOn(){
  server.send(200, "text/html", SendHTML());
  ov65->heaterRun();
}

void handle_heaterTunrOff(){
  server.send(200, "text/html", SendHTML());
  ov65->heaterStop();
}

void handle_NotFound(){
  server.send(404, "text/plain", "Not found");
}

String SendHTML(){
  String ptr = "<!DOCTYPE html> <html>\n";
  ptr +="<meta http-equiv=\"Content-type\" content=\"text/html; charset=utf-8\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  ptr +="<title>Управление отопителем</title>\n";
  ptr +="<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
  ptr +="body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;} h3 {color: #444444;margin-bottom: 50px;}\n";
  ptr +=".button {display: block;width: 80px;background-color: #3498db;border: none;color: white;padding: 13px 30px;text-decoration: none;font-size: 25px;margin: 0px auto 35px;cursor: pointer;border-radius: 4px;}\n";
  ptr +=".button-on {background-color: #3498db;}\n";
  ptr +=".button-on:active {background-color: #2980b9;}\n";
  ptr +=".button-off {background-color: #34495e;}\n";
  ptr +=".button-off:active {background-color: #2c3e50;}\n";
  ptr +="p {font-size: 14px;color: #888;margin-bottom: 10px;}\n";
  ptr +="</style>\n";
  ptr +="</head>\n";
  ptr +="<body>\n";
  ptr +="<h1>ОВ-65 дистанционное управление</h1>\n";
  switch (ov65->getState()){
    case 0:
    {
      ptr +="<p>Состояние Отопителя: ВЫКЛЮЧЕН.</p><a class=\"button button-on\" href=\"/heaterTunrOn\">ВКЛ.</a>\n";
      break;
    }
    case 1:
    {
      ptr +="<p>Состояние Отопителя: ВКЛЮЧАЕТСЯ.</p><a class=\"button button-off\" href=\"/heaterTunrOff\">ВЫКЛ.</a>\n";
      break;
    }    
    case 2:
    {
      ptr +="<p>Состояние Отопителя: ВКЛЮЧЕН.</p><a class=\"button button-off\" href=\"/heaterTunrOff\">ВЫКЛ.</a>\n";
      break;
    }
    case 3:
    {
      ptr +="<p>Состояние Отопителя: ВЫКЛЮЧАЕТСЯ.</p><a class=\"button button-off\" href=\"/heaterTunrOff\">ВЫКЛ.</a>\n";
      break;
    }
  }
  ptr += "<meta http-equiv=\"refresh\" content=\"1; URL=/\">\n";
  ptr +="</body>\n";
  ptr +="</html>\n";
  return ptr;
}

void setup() {
}

void loop() {
  // put your main code here, to run repeatedly:
  // put your setup code here, to run once:
  pinMode(2, OUTPUT);

  Relay *power = new Relay(powerPin);
  Motor *motor = new Motor(switchPin);
  Relay *ignitor = new Relay(ignitorPin);
  Relay *valve = new Relay(valvePin);

  ov65 = new Heater(power, motor, ignitor, valve);
  
  WiFi.softAP(ssid, passworg);
  WiFi.softAPConfig(local_ip, gateway, subnet);
  delay(100);
  server.on("/", handle_OnConnect);
  server.on("/heaterTunrOff", handle_heaterTunrOff);
  server.on("/heaterTunrOn", handle_heaterTunrOn);
  server.onNotFound(handle_NotFound);
  server.begin();

  while(true){
    server.handleClient();
  }
}
