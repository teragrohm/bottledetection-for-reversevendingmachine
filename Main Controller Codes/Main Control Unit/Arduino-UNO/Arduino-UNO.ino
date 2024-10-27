#include <SoftwareSerial.h>
#include <Servo.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <EEPROM.h>

LiquidCrystal_I2C lcd(0x27, 20, 4);
SoftwareSerial gsmModule(3,2);
Servo feeder;
Servo dispenser;

int buzzer_pin = 8;
int proximity = 10;

int multiplier[3] = {1, 0, 1};

int previous_millis = 0;
//int valid_inserted = 7;
uint8_t valid_inserted = 0;
int max_bottles = 5;
int ballpen_count = 3;
bool reward;
bool full_of_bottles = false;

String data;
String displayIP = "To enter new WiFi details go to http://192.168.10.80:100 To start image processing go to http://";
String defaultIP = "192.168.10.80:100";
String cameraIP;

//bool val;

int feederActuator(int num) {

  //int multiplier[3] = {1, 1, 1};
  int i;
  int servo_position;

//  result = num;

  /* if (num == 0) 
  {
    multiplier[1] -= 1;
  } */
  if (num == 1) 
  {
    multiplier[1] += 2;
  }

  Serial.println(num);

  for (i = 0; i < 3; i++) {

    servo_position = multiplier[i] * 90;
    Serial.print("Servo will now turn ");
    Serial.print(servo_position);
    Serial.println(" degrees.");
    feeder.write(servo_position);
    //delay(3000 + (3000 * multiplier[i]));
    delay(3000);
  }
  multiplier[1] = 0;
  return servo_position;
}

void resetBottleCount() {

  lcd.setCursor(0,0);
  lcd.print("0");
  lcd.setCursor(1,0);
  lcd.print(valid_inserted);
  lcd.setCursor(3,0);
  //lcd.print("out of 10");
  lcd.print("out of 5");
  lcd.setCursor(13,0);
  lcd.print("BOTTLES");
  lcd.setCursor(3,1);
  lcd.print("to get a reward");
}

void updateBottleCount(int num) {

  if (feederActuator(num) == 90) {

    if (valid_inserted < max_bottles) {

      valid_inserted += 1;
      //valid_inserted = 0;
      Serial.print("Bottles inserted: ");
      Serial.println(valid_inserted);

      lcd.setCursor(1,0);
    }
    if (valid_inserted == max_bottles) {
      lcd.setCursor(0,0);
      reward = true;
   }
    // Write number of bottles to LCD Display
    lcd.print(valid_inserted);   
  }
  EEPROM.write(10, valid_inserted);
}

void dispenseReward() {

  Serial.println("\nAnother set of 10 bottles were inserted!");
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("DISPENSING 1 BALLPEN");
  lcd.setCursor(0,2);
  lcd.print("Collect your reward!");

  dispenser.write(90);
  delay(500);
  dispenser.write(0);
  delay(500);

  ballpen_count -= 1;
  delay(3000);
  Serial.println(ballpen_count);
  EEPROM.write(5, ballpen_count);
/*
  if (ballpen_count == 0) {
    resetBallpenCount();
  }
*/
  raiseAlert();
  delay(2000);

  valid_inserted = 0;
  EEPROM.write(10, valid_inserted);
  lcd.clear();
  resetBottleCount();
  reward = false;
}

void raiseAlert() {

  gsmModule.println("AT+CMGF=1"); // Configuring TEXT mode
  updateSerial();
  gsmModule.println("AT+CMGS=\"+639302641256\""); //change ZZ with country code and xxxxxxxxxxx with phone number to sms
  delay(500);

  if (ballpen_count == 0) {

    gsmModule.print("Reverse vendo ballpen bin is empty! Please refill it."); // SMS alert
    updateSerial();
    gsmModule.write(26);

    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("THE BALLPEN BIN");
    lcd.setCursor(0,1);
    lcd.print("IS EMPTY!");
    lcd.setCursor(0,2);
    lcd.print("Please refill it.");
  }
  if (full_of_bottles) {

    gsmModule.print("Reverse vendo bottle bin is full! Please collect inserted bottles."); // SMS alert
    updateSerial();
    gsmModule.write(26);

    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("THE BOTTLE BIN");
    lcd.setCursor(0,1);
    lcd.print("IS FULL!");
    lcd.setCursor(0,2);
    lcd.print("Please empty it.");
  }

if (ballpen_count == 0 || full_of_bottles) {
  for (int j = 0; j < 5; j++) {
    for (int i = 0; i < 3; i++) {

      bool pin_stat = multiplier[i] == 1;
      digitalWrite(buzzer_pin, pin_stat);
      delay(500);
    }
  } 
 }
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

void scrollText(int row, String message, int interval, int col) {

  // for (int i = 0; i < col; i++) {
  //  message = " " + message;  
  //} 
  //message = message + " "; 
  for (int position = 0; position < (message.length() + 1 - col); position++) {

    if (Serial.available() > 0) break;

    lcd.setCursor(0, row);
    lcd.print(message.substring(position, position + col));
    delay(interval);

  }
}

void updateSerial()
{
  delay(500);
  while (Serial.available()) 
  {
    gsmModule.write(Serial.read());//Forward what Serial received to Software Serial Port
  }
  while(gsmModule.available()) 
  {
    Serial.write(gsmModule.read());//Forward what Software Serial received to Serial Port
  }
}

void setup(){
	
  Serial.begin(9600);
  gsmModule.begin(9600);
  valid_inserted = 0;
  EEPROM.write(10, valid_inserted);
  valid_inserted = EEPROM.read(10);
  ballpen_count = EEPROM.read(5);
  
  if (!(ballpen_count > 0)) {
    
    EEPROM.write(5,3);
    ballpen_count = EEPROM.read(5);
  }

  Serial.print("Ballpens: ");
  Serial.println(ballpen_count);
  delay(1000);

  feeder.attach(7);
  feeder.write(90);

  dispenser.attach(9);
  dispenser.write(0);

  pinMode(buzzer_pin, OUTPUT);
  digitalWrite(buzzer_pin, LOW);

  pinMode(proximity, INPUT);

  lcd.init();					
  lcd.backlight();
  resetBottleCount();

  gsmModule.println("AT"); 
  updateSerial();
}

void loop(){

  scrollText(3, displayIP + cameraIP, 250, 20);
  if (!(Serial.available() > 0)) delay(5000);
  
	
String currentIP;

	//while(ArduinoUno.available()>0){
  while(Serial.available()>0){
  //int val = ArduinoUno.parseInt();
  //if(ArduinoUno.read()== '\n'){
  int result;

  char received = Serial.read();
  //data += received;
  
  if (received == '\n') {

    //data[data.length()] = 0;
    //Serial.println(cameraIP);

    Serial.println(data);
    Serial.println(currentIP);

    if (data == "Invalid") 
    {
      result = 0;
      feederActuator(result);
      invalidWarning();
    }

    else if (data == "Valid") 
    {
      result = 1;
      updateBottleCount(result);
      delay(1000);
    }

    cameraIP = currentIP;
    data = "";
    currentIP = "";
    //cameraIP = "";    
  }
  else if (data.indexOf("CameraIP:") >= 0) 
  {
    currentIP += received;
  }
  else
  {
    data += received;
  }

  
/*
  if(Serial.read()== '\n'){
    Serial.println(val);
   if (val == 0) {
    //if (val.indexOf("Invalid") >= 0) {
      Serial.println("Invalid");
      //feederActuator(val);
      //invalidWarning();
    }
   /*if (val == 1) {
      // feeder.write(180);
      Serial.println("Valid");
      updateBottleCount(val);      
    }*/
	 //} 
   if (reward) {
    dispenseReward();
   }

   while (digitalRead(proximity) == HIGH && (millis() - previous_millis >= 10000))
   {
    if (digitalRead(proximity) == LOW) break;
   }
   if (digitalRead(proximity) == HIGH) full_of_bottles = true;
 }

//delay(30);
}
