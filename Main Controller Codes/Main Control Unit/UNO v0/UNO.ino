#include <SoftwareSerial.h>
#include <Servo.h>

SoftwareSerial ArduinoUno(3,2);
Servo feeder;

void setup(){
	
	Serial.begin(9600);
	ArduinoUno.begin(4800);

  feeder.attach(7);
  feeder.write(0);
}

void loop(){
	
	while(ArduinoUno.available()>0){
	//float val = ArduinoUno.parseFloat();
  int val = ArduinoUno.parseInt();
  //String val = ArduinoUno.readString();
	if(ArduinoUno.read()== '\n'){
	//Serial.println(val);
    if (val == 0) {
      Serial.println("Invalid");
    }
    if (val == 1) {
      feeder.write(180);
      Serial.println("Valid");
    }
	}
}
//delay(30);
delay(1000);
feeder.write(0);
}
