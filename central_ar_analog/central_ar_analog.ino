/*
			ATENÇÂO: ESSE CODIGO ESTA DEPRECIADO

*/



#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#define MQ_analog 35

const bool dev = false;

//rede de produção
const char* ssid = "";
const char* psw = "";

const bool sendJson = true;
const int pinLeds[] = {32,33};
String base_url;
String iot_url_token;
String iot_url_send;

String token = "";
String status = "";

int delaySend = 5000;


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

	if(dev){
		base_url = "http://192.168.0.4:3001/api/v1/ws/";
	} else {
		base_url = "https://iot-central.herokuapp.com/api/v1/ws/";
	}

	iot_url_token = base_url + "auth/devices/";
	iot_url_send =  base_url + "sensors";
	
	pinMode(pinLeds[0],OUTPUT);//ligado
	pinMode(pinLeds[1],OUTPUT);//Transferencia

	digitalWrite(pinLeds[0], HIGH);
	digitalWrite(pinLeds[1], HIGH);
	delay(2000);
	Serial.begin(9600);	
	Serial.println("Iniciando");
                  
	WiFi.mode(WIFI_STA);
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

	if(dev){
		http.begin(url);	
	} else {
		http.begin(url, "08:3B:71:72:02:43:6E:CA:ED:42:86:93:BA:7E:DF:81:C4:BC:62:30");	
	}
	
	http.addHeader("Content-Type", "application/json");

	int httpCode = 0;
	if(method == "post"){
		httpCode = http.POST(JSONmessageBuffer); 	
	} else if(method == "get"){
		httpCode = http.GET(); 	
	}

	Serial.println(url);
	//Serial.println(WiFi.macAddress().c_str());
	
	if(httpCode == 200){ //Tudo enviado
		const size_t capacity = JSON_OBJECT_SIZE(2) + 220;
		StaticJsonDocument<capacity> doc;
			
		deserializeJson(doc, http.getString());
		
		token = doc["token"].as<String>(); 
		status = doc["status"].as<String>(); 

		Serial.println(status);
		Serial.println(token);
		ledShow(1, 10, 150);
	} else {
		status = "";
		ledShow(1, 6, 350);
		Serial.println(httpCode); 
		Serial.println(http.errorToString(httpCode).c_str());
	}

	
	http.end(); 
}

void checkNet(){
	if(WiFi.status() != WL_CONNECTED){ //Verifica conexão com a internet
		Serial.println();
		Serial.println("Conectando a rede");	
		Serial.print("MAC: ");
		Serial.println(WiFi.macAddress().c_str());
	  	do{
			WiFi.begin(ssid, psw);
			for(int j = 0; j < 100; j++) {
				if(WiFi.status() == WL_CONNECTED){
					break;
				} 
				Serial.print(".");
				
				ledShowBoth(3, 150);
			}  	
	  	}while(WiFi.status() != WL_CONNECTED);
	  	
		digitalWrite(pinLeds[0],HIGH);
		digitalWrite(pinLeds[1],LOW);	
	  	Serial.println();
		Serial.println("Conectado");	
	}	
}

void loop() {
	
	checkNet();//Verifica a conexaão com a internet

	if(sendJson && status == ""){
	 	getToken();
	 }

	int sensor = analogRead(MQ_analog);
	Serial.println(sensor);
	if(sensor != 0){
		/*
		*
		*AREA DE COLETA DE DADOS DO SENSOR
		*
		*/

		int valor_analog = map(sensor, 0, 1024, 10, 1000); 

		const size_t capacity = JSON_ARRAY_SIZE(1) + 2*JSON_OBJECT_SIZE(2);
		DynamicJsonDocument JSONencoder(500);	
		char JSONmessageBuffer[500] = "";

		JSONencoder["token"] = "Bearer " + token;
		JsonArray nested = JSONencoder.createNestedArray("sensorData");
		JsonObject after[1] = nested.createNestedObject();
		after[0]["type"] = "co2";
		after[0]["value"] = valor_analog;
		//after[1]["type"] = "acy";
		//after[1]["value"] = AcY;
	
		serializeJson(JSONencoder, JSONmessageBuffer);
		Serial.println(JSONmessageBuffer);	
	

		if(sendJson){
			httpRequest(iot_url_send, JSONmessageBuffer, "post");//Envia os dados para o servidor
		}
			
	} else {
		ledShowAlter(20, 150);
		Serial.println("Erro ao ler sensor");	
	}
	
	delay(delaySend);
}
