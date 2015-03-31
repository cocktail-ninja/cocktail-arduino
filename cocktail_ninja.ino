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
Pump pumps[4] = {Pump(4, 2), Pump(5, 2), Pump(6, 2), Pump(7, 52)};

byte isBusy() {
  for(int i = 0; i < 4; i++){
    if (pumps[i].isBusy()) {
      return true;
    }
  }
  return false;
}

void process(YunClient client) {
  if (!client) return;
  
  String command = client.readStringUntil('/');
  
  if (command.startsWith("status")){
    printHeader(client, 200);
    if (isBusy()){
        client.print("{status: 'busy'}");    
    } else if(getDistance() > 5) {    
        client.print("{status: 'glass not found'}");    
    } else {
        client.print("{status: 'ready'}");      
    }
  } else if (command.equals("make_drink")){
      if (isBusy()){
        printHeader(client, 503);
        client.print("{status: 'busy'}");    
      } else if(getDistance() > 5) {    
        printHeader(client, 404);
        client.print("{status: 'glass not found'}");
      } else {
        printHeader(client, 200);        
        
        int pumpNumber[4] = {0, 0, 0, 0};
        int amounts[4] = {0, 0, 0, 0};
        
        for (int i = 0; client.available(); i++) {
          pumpNumber[i] = client.parseInt();
          client.readStringUntil('-');
          amounts[i] = client.parseInt();
          client.readStringUntil('/');
        }
      
        String pouringResponse = "";
        unsigned long maxPouringTime = 0L;
        
        for (int i = 0; i < 4; i ++) {
          if (amounts[i] && pumpNumber[i]) {
            unsigned long pouringTime = pumps[pumpNumber[i] - 1].pourMilliliters(amounts[i]);
            if (i > 0) {
              pouringResponse = pouringResponse + ", ";
            }
            pouringResponse = pouringResponse + " " + String(pumpNumber[i]) + ": " + (pouringTime);
            maxPouringTime = maxx(maxPouringTime, pouringTime);
          }
        }
        pouringResponse = pouringResponse + ", max: " + String(maxPouringTime); 

        client.print("{ pouringTime: { " +  pouringResponse +" } }");       
    }
  } else {
      printHeader(client, 405);
      client.print("{ status: 'not recognized'}");
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
