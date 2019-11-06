#define MQ_analog 14
#define FILTER 10
int valor_analog;
int valor_dig;
int minValue = 1000;
int maxValue = 0;
int arr[FILTER];
 //0 - 4095
void setup() {
   Serial.begin(9600);
  //pinMode(MQ_analog, INPUT);
   //pinMode(MQ_dig, INPUT);
	for(int i = 0; i < FILTER; i++){
		arr[i] = 0;
	}
}
 
void loop() {

   int sensor = analogRead(MQ_analog);
   valor_analog = map(sensor, 0, 1024, 10, 1000); 


	arr.push(valor_analog);
   if(valor_analog > maxValue) maxValue = valor_analog;
   if(valor_analog < minValue) minValue = valor_analog;
  // valor_dig = digitalRead(MQ_dig);
 
   Serial.print(valor_analog);
   Serial.print(" ppm atual - ");
   Serial.print(maxValue);
   Serial.print(" ppm maximo - ");
   Serial.print(minValue);
   Serial.print(" ppm minimo - ");
   Serial.print(sensor);
   Serial.print(" sensor");
   Serial.println();
  // Serial.print(" || ");
  // if(valor_dig == 0)
  //   Serial.println("GAS DETECTADO !!!");
  // else 
  //   Serial.println("GAS AUSENTE !!!");
   delay(500);
}
