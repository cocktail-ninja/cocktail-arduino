#include <Bridge.h>
#include <YunServer.h>
#include <YunClient.h>

unsigned long maxx(unsigned long a, unsigned long b){
  return (a > b)? a : b;
}

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

    unsigned long pourMilliliters(unsigned long milliliters) {
      return pour(milliliters * mlToMsConstant); 
    }
    
    unsigned long pour(unsigned long milliseconds) {
      if (!isPouring) {
        startPouring();
        shouldStopPouringAt = millis() + milliseconds;
      }
      return milliseconds;
    }

    void loop(){
      if (isPouring && (millis() > shouldStopPouringAt)) {
        stopPouring();
      }
    }
    
    byte isBusy(){
      return isPouring;
    }
};

YunServer server;

#define NumOfPumps 5
int PUMP_1 = 2;
int PUMP_2 = 3;
int PUMP_3 = 4;
int PUMP_4 = 5;
int PUMP_5 = 6;

int PUMP_FLOWRATE = 2;
int PINGPIN = 11;

Pump pumps[NumOfPumps] = {
  Pump(PUMP_1, PUMP_FLOWRATE), 
  Pump(PUMP_2, PUMP_FLOWRATE), 
  Pump(PUMP_3, PUMP_FLOWRATE), 
  Pump(PUMP_4, PUMP_FLOWRATE), 
  Pump(PUMP_5, PUMP_FLOWRATE)
};

byte isBusy() {
  for(int i = 0; i < NumOfPumps; i++){
    if (pumps[i].isBusy()) {
      return true;
    }
  }
  return false;
}

bool isGlassNotFound() {
  return getDistance() > 5;
}

void printStatus(YunClient client, String key, String value) {
  client.print("{\"" + key + "\": \"" + value + "\"}");    
}

void process(YunClient client) {
  if (!client) return;
  
  String command = client.readStringUntil('/');
  
  if (command.startsWith("status")){
    printHeader(client, 200);
    if (isBusy()){
      printStatus(client, "status", "busy");
    } else if (isGlassNotFound()) { 
      printStatus(client, "status", "glass not found"); 
    } else {
      printStatus(client, "status", "ready");
    }
  } else if (command.equals("make_drink")){
      if (isBusy()){
        printHeader(client, 503);        
        printStatus(client, "status", "busy");
      } else if(isGlassNotFound()) {    
        printHeader(client, 404);
        printStatus(client, "status", "glass not found");
      } else {
        printHeader(client, 200);        

        int pumpNumber[NumOfPumps] = {0, 0, 0, 0, 0};
        int amounts[NumOfPumps] = {0, 0, 0, 0, 0};
        
        for (int i = 0; client.available(); i++) {
          pumpNumber[i] = client.parseInt();
          client.readStringUntil('-');
          amounts[i] = client.parseInt();
          client.readStringUntil('/');
        }
      
        String pouringResponse = "";
        unsigned long maxPouringTime = 0L;
        
        for (int i = 0; i < NumOfPumps; i ++) {
          if (amounts[i] && pumpNumber[i]) {
            unsigned long pouringTime = pumps[pumpNumber[i] - 1].pourMilliliters(amounts[i]);
            if (i > 0) {
              pouringResponse = pouringResponse + ", ";
            }
            pouringResponse = pouringResponse + " \"" + String(pumpNumber[i]) + "\": " + (pouringTime);
            maxPouringTime = maxx(maxPouringTime, pouringTime);
          }
        }
        pouringResponse = pouringResponse + ",\"max\": " + String(maxPouringTime); 

        client.print("{ \"pouringTime\": { " +  pouringResponse +" } }");       
    }
  } else {
      printHeader(client, 405);
      printStatus(client, "status", "not recognized");
  }
  
  client.stop();
}

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
