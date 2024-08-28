#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>

SoftwareSerial NodeMCU(D6,D7);

void setup(){
	Serial.begin(9600);
	NodeMCU.begin(4800);
	//pinMode(D2,INPUT);
	//pinMode(D3,OUTPUT);
}

void loop(){
	int i = 0;
	NodeMCU.print(i);
	NodeMCU.println("\n");
	//delay(30);
  delay(1000);
  i = 1;
	NodeMCU.print(i);
	NodeMCU.println("\n");
  delay(1000);
}