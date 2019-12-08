#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_CCS811.h>

Adafruit_CCS811 ccs;
//Rede de produção
const char* ssid_production = "";
const char* psw_production = "";

//Rede de teste
const char* ssid_development = "";
const char* psw_development = "";

const char* version = "v1";

const bool ledsEnabled = true;
const bool sensorEnabled = true;
const bool sendJson = true;
const bool network = true;
const bool testLocal = false;
const bool sleep = true;

const int pinLeds[] = {1,3};
const String fingerprint = "08:3B:71:72:02:43:6E:CA:ED:42:86:93:BA:7E:DF:81:C4:BC:62:30";
String remoteAddress = "https://iot-central.herokuapp.com/";
String localAddress = "http://192.168.0.4:3001/";

String base_url = "api/v1/ws/";
String iot_url_token = "auth/devices/";
String iot_url_send =  "sensors";

String token = "";
String statusMode = "";
int sendTime = 60000; //Segundos
int timeSleep = 60; //Minutos

const int filterSize = 10;
int filter[filterSize];

const int valueReset = 100;
int contReset = 0;

void ledShow(int id, int size, int speed){
	digitalWrite(pinLeds[id], HIGH);
	for(int i = 0; i < size; i++){
		digitalWrite(pinLeds[id], !digitalRead(pinLeds[id]));
		delay(speed);
	}
	digitalWrite(pinLeds[id], LOW);
}

void ledShowBoth(int size, int speed){
	digitalWrite(pinLeds[0], HIGH);
	digitalWrite(pinLeds[1], HIGH);
	for(int i = 0; i < size; i++){
		digitalWrite(pinLeds[0], !digitalRead(pinLeds[0]));
		digitalWrite(pinLeds[1], !digitalRead(pinLeds[1]));
		delay(speed);
	}
	digitalWrite(pinLeds[0], HIGH);
	digitalWrite(pinLeds[1], LOW);
}

void ledShowAlter(int size, int speed){
	digitalWrite(pinLeds[0], HIGH);
	digitalWrite(pinLeds[1], LOW);
	for(int i = 0; i < size; i++){
		digitalWrite(pinLeds[0], !digitalRead(pinLeds[0]));
		digitalWrite(pinLeds[1], !digitalRead(pinLeds[1]));
		delay(speed);
	}
	digitalWrite(pinLeds[0], HIGH);
	digitalWrite(pinLeds[1], LOW);
}

void setup() {

	ESP.wdtDisable();
	ESP.wdtEnable(WDTO_8S);

	base_url = (testLocal ? localAddress + base_url : remoteAddress + base_url);
	iot_url_send = base_url + iot_url_send;
	iot_url_token = base_url + iot_url_token;

	if(ledsEnabled){
		pinMode(pinLeds[0],OUTPUT);//ligado
		pinMode(pinLeds[1],OUTPUT);//Transferencia

		digitalWrite(pinLeds[0], HIGH);
		digitalWrite(pinLeds[1], HIGH);
		delay(2000);
	} else {
		Serial.begin(115200);	
		Serial.println("\n\niniciando");
		Serial.println(base_url);
	}	

	WiFi.mode(WIFI_STA);

	if(!ledsEnabled) Serial.println(WiFi.macAddress().c_str());
	if(sensorEnabled) {
		while(!ccs.begin()){	
			if(ledsEnabled){ 
				ledShowAlter(10, 350);
			} else {
				Serial.println("Acessando sensor");
				delay(1000);
			}
		}	
		//CCS811_DRIVE_MODE_250MS
		ccs.setDriveMode(CCS811_DRIVE_MODE_250MS);

		int cont = 0;
		do{
			if(ccs.available() && !ccs.readData() && ccs.geteCO2() > 0) {
				filterPush(ccs.geteCO2());
				cont++;
			}
			if(ledsEnabled) {
				ledShow(1, 4, 80);
			} else {
				Serial.print("Recebendo valor: ");
				Serial.print(cont);
				Serial.println(" do sensor!");
				delay(320);
			}
		}while(cont < filterSize-1);
	}
}

void httpRequest(String url,String method, char JSONmessageBuffer[] = ""){
	HTTPClient http;

	testLocal ? http.begin(url) : http.begin(url, fingerprint);
	
	http.addHeader("Content-Type", "application/json");

	int httpCode = 0;
	//ESP.wdtDisable();
	if(method == "post"){
		http.addHeader("authentication", "Bearer " + token);
		httpCode = http.POST(JSONmessageBuffer); 	
	} else if(method == "get"){
		httpCode = http.GET(); 	
	}

	if(!ledsEnabled){
		Serial.println(url);
	}
	
	if(httpCode == 200){ //Tudo enviado
		const size_t capacity = JSON_OBJECT_SIZE(4) + 250;
		StaticJsonDocument<capacity> doc;
			
		deserializeJson(doc, http.getString());
		
		statusMode = doc["statusMode"].as<String>(); 
		token = doc["token"].as<String>(); 
		sendTime = doc["sendTime"].as<int>() * 1000;
		timeSleep = doc["timeSleep"].as<int>() * 60000 + (2 * sendTime);

		if(!ledsEnabled){
			Serial.println(statusMode);
			Serial.println(sendTime);
			Serial.println(timeSleep);
		} else {
			ledShow(1, 10, 150);	
		}
	} else {
		statusMode = "";
		if(ledsEnabled){
			ledShow(1, 6, 350);
		} else {
			Serial.println(httpCode); 
			Serial.println(http.errorToString(httpCode).c_str());
		}
	}

	
	http.end(); 
}

void modeSleep(int time) {

	if(sleep){
		if(ledsEnabled){
			digitalWrite(pinLeds[0], LOW);
			digitalWrite(pinLeds[1], LOW);
		} else {
			Serial.println("Mode Sleep: Actived");
		}
		ccs.setDriveMode(CCS811_DRIVE_MODE_IDLE);
		WiFi.forceSleepBegin(0);
		delay(time);
		WiFi.forceSleepWake();
		ccs.setDriveMode(CCS811_DRIVE_MODE_250MS);
		if(ledsEnabled) {
			digitalWrite(pinLeds[0], HIGH);
		} else {
			Serial.println("Mode Sleep: Desactived");
		}	
	} else {
		delay(time);
	}
}

void getSettings() {
	String tmp = iot_url_token;
	tmp.concat(WiFi.macAddress().c_str());
	int cont = 0;
	do{
		httpRequest(tmp, "get");//Efetua a requisição no servidor	
		if(WiFi.status() != WL_CONNECTED){
			break;
		}

		if(statusMode == "paused") {
			modeSleep(timeSleep);
		}
		delay(100);
		cont++;
		if(cont > 10) {
			modeSleep(sendTime);
			ESP.reset();
		}
	}while(statusMode != "ok");		
}

void filterPush(int value) {
	for(int i = 0; i < filterSize; i++) {
		filter[i] = filter[i+1];
	}
	filter[filterSize-1] = value;
}

int getFilter() {
	int max = 0;
	for(int i = 0; i < filterSize; i++) {
		max += filter[i];
	}

	return max / filterSize;
}

void checkNet(){
	if(!ledsEnabled) Serial.println("Verificando internet");
	if(WiFi.status() != WL_CONNECTED){ //Verifica conexão com a internet
		if(!ledsEnabled){
			Serial.println();
			Serial.println("Conectando a rede");	
			Serial.print("MAC: ");
			Serial.println(WiFi.macAddress().c_str());
		}

		int attemptsCount = 0;
	  	do{	
			WiFi.begin((testLocal) ? ssid_development : ssid_production, (testLocal) ? psw_development : psw_production);
			/* if(contReset == 0) {
			} else {
				WiFi.reconnect();
			} */
			for(int j = 0; j < 10; j++) {
				if(WiFi.status() == WL_CONNECTED){
					break;
				} else if(!ledsEnabled){
					Serial.print(".");	
					delay(400);
				} else {
					ledShowBoth(4, 100);	
				}
			}
			attemptsCount++;
			if(attemptsCount > 4){
				modeSleep(15*1000);
				ESP.reset();
			}
	  	}while(WiFi.status() != WL_CONNECTED);

	 	delay(100);
	 	if(!ledsEnabled){
	 		Serial.println();
	  		Serial.println("Conectado");	
	 	} else {
	 		digitalWrite(pinLeds[0],HIGH);
	 		digitalWrite(pinLeds[1],LOW);	
	 	}
	}	
}

void loop() {
	contReset++;
	ESP.wdtFeed();

	if(network){
		checkNet();//Verifica a conexão com a internet
	}

	if(sendJson){
	 	getSettings();
	 }
	
	if(sensorEnabled && statusMode == "ok"){
		if(ccs.available() && !ccs.readData()){
			
			filterPush(ccs.geteCO2());

			const size_t capacity = JSON_ARRAY_SIZE(1) + 2*JSON_OBJECT_SIZE(2);
			DynamicJsonDocument JSONencoder(500);	
			char JSONmessageBuffer[500] = "";

			JsonArray nested = JSONencoder.createNestedArray("sensorData");
			JsonObject after[1] = nested.createNestedObject();
			after[0]["type"] = "co2";
			after[0]["value"] = getFilter();
			//after[1]["type"] = "acy";
			//after[1]["value"] = AcY;
		
			serializeJson(JSONencoder, JSONmessageBuffer);
			if(!ledsEnabled){
				Serial.println(JSONmessageBuffer);	
			}
		

			if(sendJson && statusMode == "ok"){
				httpRequest(iot_url_send, "post", JSONmessageBuffer);//Envia os dados para o servidor
			}
				
		} else {
			if(ledsEnabled){
				ledShowAlter(20, 150);
			} else {
				Serial.println("Erro ao ler sensor");	
			}
		}
	}

	if(!ledsEnabled){
		Serial.println(ESP.getVcc());
		Serial.println(contReset);
	} 

	modeSleep(sendTime);
	delay(0);

	if(contReset > valueReset) {
		ESP.reset();
	}
}
