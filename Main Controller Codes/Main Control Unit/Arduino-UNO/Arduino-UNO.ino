#include <SoftwareSerial.h>
#include <Servo.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>

LiquidCrystal_I2C lcd(0x27, 20, 4);
SoftwareSerial ArduinoUno(3,2);
Servo feeder;

int valid_inserted = 8;
int max_bottles = 10;
int ballpen_count = 5;
bool reward;

//bool val;

int feederActuator() {

  int multiplier[3] = {0, 1, 0};
  int i;
  int servo_position;

  for (i = 0; i < 3; i++) {

    servo_position = multiplier[i] * 180;
    Serial.print("Servo will now turn ");
    Serial.print(servo_position);
    Serial.println(" degrees.");
    feeder.write(servo_position);
    delay(3000 + (3000 * multiplier[i]));
  }
  return servo_position;
}

void resetBottleCount() {

  lcd.setCursor(14,0);
  lcd.print("0");
  lcd.setCursor(15,0);
  lcd.print(valid_inserted);
  lcd.setCursor(10,1);
  lcd.print("out of 10");
  lcd.setCursor(9,2);
  lcd.print("BOTTLES for");
  lcd.setCursor(10,3);
  lcd.print("a  reward");
}

void updateBottleCount() {

  if (feederActuator() == 0) {

    if (valid_inserted < max_bottles) {

      valid_inserted++;
      Serial.print("Bottles inserted: ");
      Serial.println(valid_inserted);

      lcd.setCursor(15,0);
    }
    if (valid_inserted == max_bottles) {
      lcd.setCursor(14,0);
      reward = true;
   }
    // Write number of bottles to LCD Display
    lcd.print(valid_inserted);   
  }
}

void dispenseReward() {

  Serial.println("\nAnother set of 10 bottles were inserted!");
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("DISPENSING 1 BALLPEN");
  lcd.setCursor(0,2);
  lcd.print("Collect your reward!");
  ballpen_count--;

  if (ballpen_count == 0) {
    resetBallpenCount();
  }

  delay(5000);

  valid_inserted = 8;
  lcd.clear();
  resetBottleCount();
  reward = false;
}

void resetBallpenCount() {

  Serial.println("An alert for empty ballpen bin will now be sent via SMS");
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("THE BALLPEN BIN");
  lcd.setCursor(0,1);
  lcd.print("IS EMPTY!");
  lcd.setCursor(0,2);
  lcd.print("Please refill it.");
}

void invalidWarning() {

  lcd.clear();
  lcd.setCursor(0,1);
  lcd.print("The inserted item");
  lcd.setCursor(0,2);
  lcd.print("is not counted");
  delay(3000);
  lcd.clear();
  resetBottleCount();
}

void setup(){
	
	Serial.begin(9600);
	ArduinoUno.begin(4800);

  feeder.attach(7);
  feeder.write(0);

  lcd.init();					
  lcd.backlight();
  resetBottleCount();
}

void loop(){
	
	//while(ArduinoUno.available()>0){
  while(Serial.available()>0){
	//float val = ArduinoUno.parseFloat();
  //int val = ArduinoUno.parseInt();
  int val = Serial.parseInt();
  //String val = ArduinoUno.readString();
	//if(ArduinoUno.read()== '\n'){
  if(Serial.read()== '\n'){
    Serial.println(val);
    if (val == 0) {
      Serial.println("Invalid");
      feederActuator();
      invalidWarning();
    }
    if (val == 1) {
      // feeder.write(180);
      Serial.println("Valid");
      updateBottleCount();      
    }
	 }
   if (reward) {
    dispenseReward();
   }
 }  
//delay(30);
}
