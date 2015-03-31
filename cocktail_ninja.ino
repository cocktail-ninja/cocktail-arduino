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

    int pourMilliliters(unsigned long milliliters) {
      return pour(milliliters * mlToMsConstant); 
    }
    
    int pour(unsigned long milliseconds) {
      if (!isPouring) {
        startPouring();
        shouldStopPouringAt = millis() + milliseconds;
      }
      return milliseconds;
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
  
  if(getDistance()<=5){
    String make_drink = client.readStringUntil('/');
    for (int i = 0; client.available(); i++) {
      pumpNumber[i] = client.parseInt();
      client.readStringUntil('-');
      amounts[i] = client.parseInt();
      client.readStringUntil('/');
    }
  
    String pouringResponse = "";
    unsigned long maxPouringTime;
    
    for (int i = 0; i < 4; i ++) {
      if (amounts[i] && pumpNumber[i]) {
        unsigned long pouringTime = pumps[pumpNumber[i] - 1].pourMilliliters(amounts[i]);
        if (i > 0) {
          pouringResponse = pouringResponse + ", ";
        }
        pouringResponse = pouringResponse + " " + String(pumpNumber[i]) + ": " + String(pouringTime);
        maxPouringTime = maxx(maxPouringTime, pouringTime);
      }
    }
    pouringResponse = pouringResponse + ", max: " + maxPouringTime; 
    printHeader(client, 200);
    client.print("{ pouringTime: { " +  pouringResponse +" } }");
  } else {
    printHeader(client, 404);
    client.print("{status:'glass not found'}");
  }
  
  client.stop();
}

int PINGPIN = 12;

long getDistance(){
  long duration,cm;
  
  pinMode(PINGPIN, OUTPUT);
  digitalWrite(PINGPIN, LOW);
  delayMicroseconds(2);
  digitalWrite(PINGPIN, HIGH);
  delayMicroseconds(5);
  digitalWrite(PINGPIN, LOW);
  pinMode(PINGPIN, INPUT);
   
  duration = pulseIn(PINGPIN, HIGH);
  cm = microsecondsToCentimeters(duration);

  return cm;
}

long microsecondsToCentimeters(long microseconds)
{
  return microseconds / 29 / 2;
}


void printHeader(YunClient client, int statusCode){
  client.println("Status: " +  String(statusCode));
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
