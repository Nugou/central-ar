#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_CCS811.h>

Adafruit_CCS811 ccs;
//ESP.deepSleep(1 * 60000000);//Dorme por 1 Minuto (Deep-Sleep em Micro segundos).
const char* ssid = "Temp";
const char* psw = "abc12345";

const bool ledsEnabled = true;
const bool sendJson = true;
const int pinLeds[] = {1,3};
String base_url = "https://iot-central.herokuapp.com/api/v1/ws/";
//String base_url = "http://192.168.0.4:3001/api/v1/ws/";
String iot_url_token = base_url + "auth/devices/";
String iot_url_send =  base_url + "sensors";

String token = "";
String status = "";

int delaySend = 30000;

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
	if(ledsEnabled){
		pinMode(pinLeds[0],OUTPUT);//ligado
		pinMode(pinLeds[1],OUTPUT);//Transferencia

		digitalWrite(pinLeds[0], HIGH);
		digitalWrite(pinLeds[1], HIGH);
		delay(2000);
	} else {
		Serial.begin(115200);	
	}	

	WiFi.mode(WIFI_STA);
	if(!ledsEnabled) Serial.println(WiFi.macAddress().c_str());
	do{	
		if(ledsEnabled){ 
			ledShowAlter(10, 350);
		} else {
			Serial.println("Acessando sensor");
			delay(1000);
		}
	}while(!ccs.begin());	
}


void getToken(){
	String tmp = iot_url_token;
	tmp.concat(WiFi.macAddress().c_str());
	do{
		httpRequest(tmp, "", "get");//Efetua a requisição no servidor	
		if(WiFi.status() != WL_CONNECTED){
			break;
		}
	}while(status != "ok");		
}


void httpRequest(String url, char JSONmessageBuffer[300], String method){
	HTTPClient http;

	http.begin(url, "08:3B:71:72:02:43:6E:CA:ED:42:86:93:BA:7E:DF:81:C4:BC:62:30");
	//http.begin(url);
	http.addHeader("Content-Type", "application/json");

	int httpCode = 0;
	if(method == "post"){
		httpCode = http.POST(JSONmessageBuffer); 	
	} else if(method == "get"){
		httpCode = http.GET(); 	
	}

	if(!ledsEnabled){
		Serial.println(url);
		//Serial.println(WiFi.macAddress().c_str());
	}
	
	if(httpCode == 200){ //Tudo enviado
		const size_t capacity = JSON_OBJECT_SIZE(2) + 220;
		StaticJsonDocument<capacity> doc;
			
		deserializeJson(doc, http.getString());
		
		token = doc["token"].as<String>(); 
		status = doc["status"].as<String>(); 

		if(!ledsEnabled){
			Serial.println(status);
			Serial.println(token);
		} else {
			ledShow(1, 10, 150);	
		}
	} else {
		status = "";
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

	  	do{
			WiFi.begin(ssid, psw);
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
	
	checkNet();//Verifica a conexão com a internet
	
	if(sendJson && status == ""){
	 	getToken();
	 }
	
	if(ccs.available() && !ccs.readData()){
		
		const size_t capacity = JSON_ARRAY_SIZE(1) + 2*JSON_OBJECT_SIZE(2);
		DynamicJsonDocument JSONencoder(500);	
		char JSONmessageBuffer[500] = "";

		JSONencoder["token"] = "Bearer " + token;
		JsonArray nested = JSONencoder.createNestedArray("sensorData");
		JsonObject after[1] = nested.createNestedObject();
		after[0]["type"] = "co2";
		after[0]["value"] = ccs.geteCO2();
		//after[1]["type"] = "acy";
		//after[1]["value"] = AcY;
	
		serializeJson(JSONencoder, JSONmessageBuffer);
		if(!ledsEnabled){
			Serial.println(JSONmessageBuffer);	
		}
	

		if(sendJson && status == "ok"){
			httpRequest(iot_url_send, JSONmessageBuffer, "post");//Envia os dados para o servidor
		}
			
	} else {
		if(ledsEnabled){
			ledShowAlter(20, 150);
		} else {
			Serial.println("Erro ao ler sensor");	
		}
	}
	
	delay(delaySend);
}
