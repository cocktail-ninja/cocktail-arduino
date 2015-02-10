#include <Bridge.h>
#include <YunServer.h>
#include <YunClient.h>

class Pump {
  private:
    unsigned long shouldStopPouringAt, mlToMsConstant;
    byte pumpPin, isPouring;

    void stopPouring() {
      digitalWrite(pumpPin, HIGH);
      isPouring = false;
    }

    void startPouring() {
      digitalWrite(pumpPin, LOW);
      isPouring = true;
    }

  public:
    Pump(int _pumpPin, double flowRate) {
      mlToMsConstant = 1000 / flowRate;
      pumpPin = _pumpPin;
      pinMode(pumpPin, OUTPUT);
      stopPouring();
    }

    void pourMilliliters(unsigned long milliliters) {
      pour(milliliters * mlToMsConstant); 
    }
    
    void pour(unsigned long milliseconds) {
      if (!isPouring) {
        startPouring();
        shouldStopPouringAt = millis() + milliseconds;
      }
    }

    void loop() {
      if (isPouring && (millis() > shouldStopPouringAt)) {
        stopPouring();
      }
    };
};

YunServer server;
Pump pumps[4] = {Pump(4, 2), Pump(5, 2), Pump(6, 2), Pump(7, 52)};

void process(YunClient client) {
  if (!client) return;

  int pumpNumber[4] = {0, 0, 0, 0};
  int amounts[4] = {0, 0, 0, 0};
  
  String make_drink = client.readStringUntil('/');
  for (int i = 0; client.available(); i++) {
    pumpNumber[i] = client.parseInt();
    client.readStringUntil('-');
    amounts[i] = client.parseInt();
    client.readStringUntil('/');
  }

  for (int i = 0; i < 4; i ++) {
    if (amounts[i] && pumpNumber[i]) {
      pumps[pumpNumber[i] - 1].pourMilliliters(amounts[i]);
    }
  }

  printHeader(client, 200);
  client.print("{}");

  client.stop();
}

void printHeader(YunClient client, int statusCode){
  client.println("Status: " +  String(statusCode));
  client.println("Access-Control-Allow-Origin: *");
  client.println("Access-Control-Allow-Methods: GET");
  client.println("Content-Type: application/json");
  client.println("Connection: close");
  client.println();
}

void setup() {
  Serial.begin(9600);
  Bridge.begin();
  server.listenOnLocalhost();
  server.begin();
}

void loop() {
  for (int i = 0; i < sizeof(pumps)/sizeof(Pump); i ++) {
    pumps[i].loop();
  }

  process(server.accept());
}
