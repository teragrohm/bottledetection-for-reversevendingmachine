#include "defines.h"

#include <SoftwareSerial.h>
#include <Servo.h>
#include <WebSockets2_Generic.h>
//#include <ESP8266WiFi.h>
#include <WiFiManager.h>

SoftwareSerial ESPSerial(D6, D7);

//bool serverReachable = false;
bool connected;

int resultCount = 0;
int reference;

Servo s1;
Servo s2;

bool valveManual = false;
bool valveAuto = true;
bool holderActivated = false;

int interval_reconnect = 30000;
int updateInterval = 5000;  // interval between updates
unsigned long lastUpdate = 0;

using namespace websockets2_generic;

WiFiManager SIBATNode;


WebsocketsClient client;
/*
class Valve
{
  int position;

public: 
  Valve()
  {
    //
  }

  void Attach(int pin)
  {
    s1.attach(pin);
  }  
  
  void Set(int angle)
  {
    s1.write(angle);      
  }  

  void manualMode() {

    valveAuto = false;
    valveManual = true;

    s1.write(0);
  }

  void moistAutoMode() {

    valveManual = false;
    valveAuto = true;

    moisture = analogRead(A0);
	
    if (moisture < 380) {
      //enoughWaterIntake = true;
      position = 180;
  }

    else {
      position = 0;
  }
  
  s1.write(position);
 }
  
};

Valve servoValve;
*/
void reconnectToServer() {

  if ((millis() - lastUpdate) > interval_reconnect) {

    lastUpdate = millis();

    if (resultCount >= reference) {
    Serial.println("Still connected");
   }
    else {
      Serial.println("Not connected to server");

      connected = client.connect(websockets_server_host, websockets_server_port, "/ws");
      
      if (connected) {
      //Serial.println("Connected!");
      client.send("Link to Arduino microcontroller established");
     }
     resultCount = 0;
   } 
   reference = resultCount + 1;
  }

  //connected = client.connect(websockets_server_host, websockets_server_port, "/ws");

  //client.send("ping");
  Serial.println(resultCount);
  Serial.println(reference);
  delay(1000);

  interval_reconnect = 20000;
}

void onEventsCallback(WebsocketsEvent event, String data) {
  (void)data;
  /*
  if (event == WebsocketsEvent::ConnectionOpened) 
  {
    //Serial.println("Connnection Opened");
  } 
  */
  if (event == WebsocketsEvent::ConnectionClosed) {
    //Serial.println("Connnection Closed");
    //serverReachable = false;
  }
  /* 
  else if (event == WebsocketsEvent::GotPing) 
  {
    //Serial.println("Got a Ping!");
  } 
  else if (event == WebsocketsEvent::GotPong) 
  {
    Serial.println("Got a Pong!");
  }
  */
}
 void onMessageCallback(WebsocketsMessage message) {
  int result;
  //Doing something with received String message.data() type

  //Serial.print("Got Message: ");
  //Serial.println(message.data());

  if (message.data() == "Valid") {

    result = 1;
    Serial.print(result);
    Serial.println("\n");
    //Serial.println(message.data());
    client.close();
    delay(interval_reconnect);
    
    interval_reconnect = 0;
    //Serial.println(interval_reconnect);
  }

  if (message.data() == "Invalid") {

    result = 0;
    Serial.print(result);
    Serial.println("\n");
    delay(10000);
  }

  resultCount++;
}


void setup() {
  Serial.begin(9600);
  ESPSerial.begin(4800);


  while (!Serial && millis() < 5000)
    ;

  // Connect to wifi
  //WiFi.begin(ssid, password);
  //WiFiManagerParameter parameter("parameterId", "Web App Host IP", "default value", 40);
  //SIBATNode.addParameter(parameter);

  SIBATNode.autoConnect("SIBAT Node 1");
  //SIBATNode.startConfigPortal("SIBAT Node 1");
  /*
  // Check if connected to wifi
  if (WiFi.status() != WL_CONNECTED) 
  {
    //Serial.println("No Wifi!");
    return;
  }
  */
  //Serial.print("Connected to Wifi, Connecting to WebSockets Server @");
  //Serial.println(websockets_server_host);

  // run callback when messages are received

  client.onMessage(onMessageCallback);

  // run callback when events are occuring
  // client.onEvent(onEventsCallback);

  // try to connect to Websockets server
  connected = client.connect(websockets_server_host, websockets_server_port, "/ws");
  //reconnectToServer();
  reference = resultCount + 1;
  /* 
  else 
  {
    //Serial.println("Not Connected!");
  }
  */
  //}
}
void loop() {

    // let the websockets client check for incoming messages
    if (client.available()) {
      client.poll();
   }
   reconnectToServer();
 
  //}  
}
