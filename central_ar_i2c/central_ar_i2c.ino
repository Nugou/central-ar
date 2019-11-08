#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_CCS811.h>

extern "C" {
  #include "gpio.h"
}

extern "C" {
  #include "user_interface.h"
}

Adafruit_CCS811 ccs;
//ESP.deepSleep(1 * 60000000);//Dorme por 1 Minuto (Deep-Sleep em Micro segundos).
const char* ssid_production = "evento";
const char* psw_production = "cesupaargo";
const char* ssid_development = "Roberval Malino";
const char* psw_development = "rm81589636";

const char* version = "v1";

const bool ledsEnabled = false;
const bool sensorEnabled = false;
const bool sendJson = true;
const bool network = true;
const bool testLocal = true;

const int pinLeds[] = {1,3};
const String fingerprint = "08:3B:71:72:02:43:6E:CA:ED:42:86:93:BA:7E:DF:81:C4:BC:62:30";
String remoteAddress = "https://iot-central.herokuapp.com/";
String localAddress = "http://192.168.0.4:3001/";

String base_url = "api/v1/ws/";
String iot_url_token = "auth/devices/";
String iot_url_send =  "sensors";

String token = "";
String statusMode = "";
int sendTime = 5; //Segundos
int timeSleep = 60; //Minutos

const int filterSize = 10;
int filter[filterSize];

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
	do{	
		if(!sensorEnabled) break;
		if(ledsEnabled){ 
			ledShowAlter(10, 350);
		} else {
			Serial.println("Acessando sensor");
			delay(1000);
		}
	}while(!ccs.begin());	

	int cont = 0;
	do{
		if(ccs.available()){
			filterPush(ccs.geteCO2());
			cont++;
		}
	}while(cont < filterSize-1);
}

void modeSleep(int time) {
	WiFi.forceSleepBegin(0);
	delay(time);
	WiFi.forceSleepWake();
}

void getSettings() {
	String tmp = iot_url_token;
	tmp.concat(WiFi.macAddress().c_str());
	do{
		httpRequest(tmp, "", "get");//Efetua a requisição no servidor	
		if(WiFi.status() != WL_CONNECTED){
			break;
		}

		if(statusMode == "paused") {
			modeSleep(timeSleep);
		}
		delay(500);
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

void httpRequest(String url, char JSONmessageBuffer[300], String method){
	HTTPClient http;

	testLocal ? http.begin(url) : http.begin(url, fingerprint);
	
	http.addHeader("Content-Type", "application/json");

	int httpCode = 0;
	if(method == "post"){
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
		timeSleep = doc["timeSleep"].as<int>() * 60000;

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

void checkNet(){
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
			for(int j = 0; j < 100; j++) {
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
			if(attemptsCount ==  10){
				WiFi.forceSleepBegin();
				delay(15 * 1000);
				ESP.restart();
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
	
	if(network){
		checkNet();//Verifica a conexão com a internet
	}

	if(sendJson){
	 	getSettings();
	 }
	
	if(sensorEnabled){
		if(ccs.available() && !ccs.readData()){
			
			filterPush(ccs.geteCO2());

			const size_t capacity = JSON_ARRAY_SIZE(1) + 2*JSON_OBJECT_SIZE(2);
			DynamicJsonDocument JSONencoder(500);	
			char JSONmessageBuffer[500] = "";

			JSONencoder["token"] = "Bearer " + token;
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
				httpRequest(iot_url_send, JSONmessageBuffer, "post");//Envia os dados para o servidor
			}
				
		} else {
			if(ledsEnabled){
				ledShowAlter(20, 150);
			} else {
				Serial.println("Erro ao ler sensor");	
			}
		}
	}
	Serial.println(ESP.getVcc());
	modeSleep(sendTime);
}
