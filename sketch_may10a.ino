#include <WiFi.h>
#include <WebServer.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// WiFi setting
const char* ssid = "OV-65";
const char* passworg = "01234567";

// IP setting
IPAddress local_ip(192,168,2,1);
IPAddress gateway(192,168,2,1);
IPAddress subnet(255,255,255,0);
WebServer server(80);

const int powerPin = 18;
const int switchPin = 19;
const int ignitorPin = 21;
const int valvePin = 22;
const int tempPin = 15;

const int ignitorDelay = 30 * 1000; // msec
const int stabilizationDelay = 30 * 1000; // msec

OneWire oneWire(tempPin);
DallasTemperature sensors(&oneWire);

DeviceAddress exhaustSensor = {0x28, 0x44, 0xC7, 0x46, 0xD4, 0x9C, 0x3C, 0x39};
DeviceAddress blowSensor = {0x28, 0xE1, 0x7D, 0x46, 0xD4, 0x5D, 0x7F, 0xA8};
DeviceAddress garageTempSensor = {0x28, 0xC7, 0xFB, 0x46, 0xD4, 0x6A, 0x19, 0x00};


// 0 = off 
// 1 = start
// 2 = on
// 3 = stop
// 4 = fail
int state = 0;

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

  public:
  Heater(Relay *_power, Motor *_motor, Relay *_ignitor, Relay *_valve){
    power = _power;
    motor = _motor;
    ignitor = _ignitor;
    valve = _valve;

    power->turnOff();
    ignitor->turnOff();
    valve->turnOff();
  }
  
  void run(){
    state = 1;
    power->turnOn();
    valve->turnOn();
    motor->HighSpeed();

    Serial2.println("before delay");
    delay(ignitorDelay);
    Serial2.println("after delay");

    ignitor->turnOn();
    digitalWrite(2, HIGH);

    delay(stabilizationDelay);

    ignitor->turnOff();
    digitalWrite(2, LOW);
    Serial2.println("before change state");
    state = 2;
    Serial2.println("after change state");

    return;
  }

  static void runImpl(void* _this){
    ((Heater*)_this)->run();
    vTaskDelete(NULL);
  }

  void heaterRun(){
    xTaskCreatePinnedToCore(this->runImpl, "ov65-run", 10000, this, 0, NULL, 0);
  }

  void stop(){
    state = 3;
    valve->turnOff();
    // 5 minutes
    delay(5  * 1000);
    power->turnOff();
    state = 0;
  }

  static void stopImpl(void* _this){
    ((Heater*)_this)->stop();
    vTaskDelete(NULL);
  }

  void heaterStop(){
    xTaskCreatePinnedToCore(this->stopImpl, "ov65-stop", 10000, this, 0, NULL, 0);
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
  switch (state){
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
  sensors.requestTemperatures();
  ptr +="<h1>Температура выхлопных газов: " + String(sensors.getTempC(exhaustSensor)) + "°C</h1>\n";
  ptr +="<h1>Температура выходного воздуха: " + String(sensors.getTempC(blowSensor)) + "°C</h1>\n";
  ptr +="<h1>Температура в гараже: " + String(sensors.getTempC(garageTempSensor)) + "°C</h1>\n";
  ptr += "<meta http-equiv=\"refresh\" content=\"1; URL=/\">\n";
  ptr +="</body>\n";
  ptr +="</html>\n";
  return ptr;
}

void setup() {
}

void loop() {
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
  sensors.begin();

  while(true){
    server.handleClient();
  }
}
