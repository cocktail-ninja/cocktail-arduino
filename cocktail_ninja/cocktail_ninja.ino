#include <Bridge.h>
#include <YunServer.h>
#include <YunClient.h>
#include <EEPROM.h>

unsigned long maxx(unsigned long a, unsigned long b){
	return (a > b)? a : b;
}

class Pump {
	private:
		unsigned long shouldStopPouringAt, shouldStartsPouringAt, mlToMsConstant;
		byte pumpPin, isPouring;
		String cid;

		void stopPouring() {
			digitalWrite(pumpPin, HIGH);
			isPouring = false;
		}

		void startPouring() {
			digitalWrite(pumpPin, LOW);
			isPouring = true;
		}

	public:
		Pump(int _pumpPin, String _cid, double flowRate) {
			mlToMsConstant = 1000 / flowRate;
			pumpPin = _pumpPin;
			cid = _cid;
			pinMode(pumpPin, OUTPUT);
			stopPouring();
		}

		unsigned long pourMilliliters(unsigned long milliliters, unsigned long startsFrom) {
			return pour(milliliters * mlToMsConstant, startsFrom);
		}

		unsigned long pour(unsigned long milliseconds, unsigned long startsFrom) {
			unsigned long currentTime = millis();
			shouldStartsPouringAt = currentTime + startsFrom;
			shouldStopPouringAt = shouldStartsPouringAt + milliseconds;
			return milliseconds;
		}

		void loop(){
			unsigned long currentTime = millis();
			if (!isPouring && currentTime > shouldStartsPouringAt && currentTime < shouldStopPouringAt) {
				startPouring();
			} else if (isPouring && (currentTime > shouldStopPouringAt)) {
				stopPouring();
			}
		}

		byte isBusy(){
			return isPouring;
		}

		String toJson() {
			return "{\"cid\":\"" + cid + "\"}";
		}
};

YunServer server;

#define NumOfAlcoholPumps 6
#define NumOfDrinkValves 4
#define NumOfIngredients 10
int PUMP_1 = 8;
int PUMP_2 = 9;
int PUMP_3 = 10;
int PUMP_4 = 11;
int PUMP_5 = 12;
int PUMP_6 = 7;
int VALVE_1 = A4;
int VALVE_2 = A3;
int VALVE_3 = A2;
int VALVE_4 = A1;

int PUMP_FLOWRATE = 2;
int VALVE_FLOWRATE = 52;
int PINGPIN = 3;

String COMPONENT_IDS[NumOfIngredients] = {"P1", "P2", "P3", "P4", "P5", "P6", "V1", "V2", "V3", "V4"};
Pump pumps[NumOfIngredients] = {
	Pump(PUMP_1, "P1", PUMP_FLOWRATE),
	Pump(PUMP_2, "P2", PUMP_FLOWRATE),
	Pump(PUMP_3, "P3", PUMP_FLOWRATE),
	Pump(PUMP_4, "P4", PUMP_FLOWRATE),
	Pump(PUMP_5, "P5", PUMP_FLOWRATE),
	Pump(PUMP_6, "P6", PUMP_FLOWRATE),
	Pump(VALVE_1, "V1", VALVE_FLOWRATE),
	Pump(VALVE_2, "V2", VALVE_FLOWRATE),
	Pump(VALVE_3, "V3", VALVE_FLOWRATE),
	Pump(VALVE_4, "V4", VALVE_FLOWRATE)
};

int findComponentIndex(String cid) {
	for (int i = 0; i < NumOfIngredients; ++i) {
		if (COMPONENT_IDS[i].equalsIgnoreCase(cid)) {
		  return i;
		}
	}
	return -1;
}

byte isBusy() {
	for(int i = 0; i < NumOfIngredients; i++){
		if (pumps[i].isBusy()) {
			return true;
		}
	}
	return false;
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

long microsecondsToCentimeters(long microseconds){
	return microseconds / 29 / 2;
}

bool isGlassNotFound() {
	return getDistance() > 5;
}

void printStatus(YunClient client, int statusCode, String key, String value) {
	printHeader(client, statusCode);
	client.print("{\"" + key + "\": \"" + value + "\"}");
}

void printHeader(YunClient client, int statusCode){
	client.println("Status: " +  String(statusCode));
	client.println("Content-Type: application/json");
	client.println("Connection: close");
	client.println();
}

int pourAlcoholPumps (int amounts[8], long startTime) {
	unsigned long totalPouringTime = 0L;
	for (int i = 0; i < NumOfAlcoholPumps; i++) {
		if (amounts[i]) {
			unsigned long pouringTime = pumps[i].pourMilliliters(amounts[i], startTime);
			totalPouringTime = maxx(totalPouringTime, pouringTime);
		}
	}
	return totalPouringTime;
}

int pourNonAlcoholValves (int amounts[8], long startTime) {
	unsigned long totalPouringTime = 0L;
	for (int i = NumOfAlcoholPumps; i < NumOfIngredients; i++) {
		if (amounts[i]) {
			unsigned long pouringTime = pumps[i].pourMilliliters(amounts[i], startTime);
			totalPouringTime = maxx(totalPouringTime, pouringTime);
		}
	}
	return totalPouringTime;
}

void processStatusResponse(YunClient client){
	if (isBusy()){
		printStatus(client, 503, "status", "busy");
	} else if (isGlassNotFound()) {
		printStatus(client, 404, "status", "glass not found");
	} else {
		printStatus(client, 200, "status", "ready");
	}
}

void processMakeDrinkResponse(YunClient client){
	if (isBusy()){
		printStatus(client, 503, "status", "busy");
	} else if(isGlassNotFound()) {
		printStatus(client, 404, "status", "glass not found");
	} else {
		int amounts[NumOfIngredients] = {0, 0, 0, 0, 0, 0, 0, 0 ,0, 0};

		while (client.available()) {
			String cid = client.readStringUntil('-');
			int pumpNumber = findComponentIndex(cid);
			if (pumpNumber < 0) {
				client.readStringUntil('/');
				continue;
			}
			amounts[pumpNumber] = client.parseInt();
			client.readStringUntil('/');
		}

		unsigned long alcoholPouringTime = pourAlcoholPumps(amounts, 0L);
		unsigned long nonAlcoholPouringTime = pourNonAlcoholValves(amounts, alcoholPouringTime);

		printStatus(client, 200, "ready_in", String(alcoholPouringTime + nonAlcoholPouringTime));
	}
}

void processIngredientsResponse(YunClient client) {
	String response = "[";
	for (int i = 0; i < NumOfIngredients; ++i) {
		response += pumps[i].toJson();
		if (i < NumOfIngredients - 1) response += ",";
	}
	response += "]";

	printHeader(client, 200);
	client.print(response);
}

void processCleanResponse(YunClient client) {
	if (isBusy()){
		printStatus(client, 503, "status", "busy");
		return;
	}

	int duration = 60;
	if (client.available()) {
		duration = client.parseInt();
		client.readStringUntil('/');
	}

	int alcoholAmount = duration * PUMP_FLOWRATE;
	int nonAlcoholAmount = duration * VALVE_FLOWRATE;
	int amounts[NumOfIngredients] = {
		alcoholAmount, alcoholAmount, alcoholAmount, alcoholAmount, alcoholAmount, alcoholAmount,
		nonAlcoholAmount, nonAlcoholAmount ,nonAlcoholAmount, nonAlcoholAmount};
	pourAlcoholPumps(amounts, 0L);
	pourNonAlcoholValves(amounts, 0L);
	printStatus(client, 200, "completed_in", String(duration));
}

void process(YunClient client) {
	if (!client) return;

	String command = client.readStringUntil('/');

	if (command.startsWith("status")){
		processStatusResponse(client);
	} else if (command.equals("make_drink")){
		processMakeDrinkResponse(client);
	} else if (command.equals("ingredients")) {
		processIngredientsResponse(client);
	} else if (command.equals("clean")) {
		processCleanResponse(client);
	} else {
		printStatus(client, 405, "status", "not recognized");
	}

	client.stop();
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
