#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_CCS811.h>

#define uS_TO_S_FACTOR 1000000

Adafruit_CCS811 ccs;
//Rede de produção
const char* ssid_production = ""; 

//Rede de teste
const char* psw_production = "";
const char* ssid_development = "";
const char* psw_development = "";

const char* version = "v1";

const bool sensorEnabled = true;
const bool sendJson = true;
const bool network = true;
const bool testLocal = false;
const bool sleepEnabled = true;
const bool filterEnabled = true;

const int attempts_request = 3;
const int pinLeds[] = {2,4};
const char* fingerprint = "08:3B:71:72:02:43:6E:CA:ED:42:86:93:BA:7E:DF:81:C4:BC:62:30";
String remoteAddress = "https://iot-central.herokuapp.com/";
String localAddress = "http://192.168.0.4:3001/";

String base_url = "api/v1/ws/";
String iot_url_token = "auth/devices/";
String iot_url_send =  "sensors";

String token = "";
String statusMode = "";
int sendTime = 6; //Segundos
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

void httpRequest(String url,String method, char JSONmessageBuffer[] = ""){
  HTTPClient http;

  testLocal ? http.begin(url) : http.begin(url, fingerprint);
  
  http.addHeader("Content-Type", "application/json");

  for(int i = 0; i < attempts_request; i++){

    checkNet();

    int httpCode = 0;

    if(method == "post"){
      http.addHeader("authentication", "Bearer " + token);
      httpCode = http.POST(JSONmessageBuffer);  
    } else if(method == "get"){
      httpCode = http.GET();  
    }

    Serial.println(url);
    
    if(httpCode == 200){ //Tudo enviado
      const size_t capacity = JSON_OBJECT_SIZE(4) + 250;
      StaticJsonDocument<capacity> doc;
        
      deserializeJson(doc, http.getString());
      
      statusMode = doc["statusMode"].as<String>(); 
      token = doc["token"].as<String>(); 
      sendTime = doc["sendTime"].as<int>();
      timeSleep = doc["timeSleep"].as<int>() * 60 + (2 * sendTime);

      Serial.println(statusMode);
      Serial.println(sendTime);
      Serial.println(timeSleep);
      ledShow(1, 10, 150);  
      break;
    } else {
      statusMode = "";
      Serial.println(httpCode); 
      Serial.println(http.errorToString(httpCode).c_str());
      ledShow(1, 6, 350);
    }
  }

  
  http.end(); 
}

void setup() {

	base_url = (testLocal ? localAddress + base_url : remoteAddress + base_url);
	iot_url_send = base_url + iot_url_send;
	iot_url_token = base_url + iot_url_token;

	pinMode(pinLeds[0],OUTPUT);//ligado
	pinMode(pinLeds[1],OUTPUT);//Transferencia

	digitalWrite(pinLeds[0], HIGH);
	digitalWrite(pinLeds[1], HIGH);
	Serial.begin(115200);	
	delay(2000);
	Serial.println("\n\niniciando");
	Serial.println(base_url);
	Serial.println(WiFi.macAddress().c_str());

	WiFi.mode(WIFI_STA);

	if(sensorEnabled) {
		while(!ccs.begin()){	
			Serial.println("Acessando sensor");
			ledShowAlter(10, 350);
		}	
		//CCS811_DRIVE_MODE_250MS
		ccs.setDriveMode(CCS811_DRIVE_MODE_250MS);

		int cont = 0;
		do{
			if(ccs.available() && !ccs.readData() && ccs.geteCO2() > 0) {
				filterPush(ccs.geteCO2());
				cont++;
			}
			ledShow(1, 4, 80);
			Serial.print("Recebendo valor: ");
			Serial.print(cont);
			Serial.println(" do sensor!");
		}while(cont < filterSize-1);
	}

	if(network){
		checkNet();//Verifica a conexão com a internet
	}

	if(sendJson){
	 	getSettings();//Recebe as configurações do servidor
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
			Serial.println(JSONmessageBuffer);	

			if(sendJson && statusMode == "ok"){
				httpRequest(iot_url_send, "post", JSONmessageBuffer);//Envia os dados para o servidor
			}
				
		} else {
			ledShowAlter(20, 150);
			Serial.println("Erro ao ler sensor");	
		}
	}
	
	//Serial.println(((float)rom_phy_get_vdd33()) / 1000);

	modeSleep(sendTime);
}

void modeSleep(int time) {

	if(sleepEnabled){
		digitalWrite(pinLeds[0], LOW);
		digitalWrite(pinLeds[1], LOW);
		Serial.flush(); 
		esp_sleep_enable_timer_wakeup(time * uS_TO_S_FACTOR);
		esp_deep_sleep_start();
	} else {
		delay(time);
	}
}

void getSettings() {
	String tmp = iot_url_token;
	tmp.concat(WiFi.macAddress().c_str());
	httpRequest(tmp, "get");//Efetua a requisição no servidor	
	if(statusMode == "paused") {
		modeSleep(timeSleep);
	}
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
	if(filterEnabled) return filter[filterSize-1];
	return max / filterSize;
}

void checkNet(){
	Serial.println("Verificando internet");
	if(WiFi.status() != WL_CONNECTED){ //Verifica conexão com a internet
		Serial.println();
		Serial.println("Conectando a rede");	
		Serial.print("MAC: ");
		Serial.println(WiFi.macAddress().c_str());

		int attemptsCount = 0;
	  	do{	
			WiFi.begin((testLocal) ? ssid_development : ssid_production, (testLocal) ? psw_development : psw_production);
			for(int j = 0; j < 10; j++) {
				Serial.print(".");	
				ledShowBoth(4, 100);	
				if(WiFi.status() == WL_CONNECTED){
					break;
				}
			}
			attemptsCount++;
			if(attemptsCount > 4){
				modeSleep(15*1000);
			}
	  	}while(WiFi.status() != WL_CONNECTED);

	 	delay(100);
		Serial.println();
		Serial.println("Conectado");	
		digitalWrite(pinLeds[0],HIGH);
		digitalWrite(pinLeds[1],LOW);	
	}	
}

void loop() {
	
}
