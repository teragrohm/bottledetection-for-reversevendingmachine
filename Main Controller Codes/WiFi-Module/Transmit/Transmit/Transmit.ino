#include "defines.h"

#include <WebSockets2_Generic.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>


bool valveManual = false;
bool valveAuto = true;
bool holderActivated = false;

int  updateInterval = 5000;   // interval between updates
unsigned long lastUpdate;

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
/*
void soilCover() {

  if (holderActivated) {

    s2.write(180);
  }

  else {

    s2.write(0);
  }

}
*/

void objectDetectionTest {

  client.send("Valid");
  delay(8000);
  client.send("Invalid");
  delay(8000);
}

/*
void onEventsCallback(WebsocketsEvent event, String data) 
{
  (void) data;
  
  if (event == WebsocketsEvent::ConnectionOpened) 
  {
    //Serial.println("Connnection Opened");
  } 
  else if (event == WebsocketsEvent::ConnectionClosed) 
  {
    //Serial.println("Connnection Closed");
  } 
  else if (event == WebsocketsEvent::GotPing) 
  {
    //Serial.println("Got a Ping!");
  } 
  else if (event == WebsocketsEvent::GotPong) 
  {
    Serial.println("Got a Pong!");
  }
}
*/
void onMessageCallback(WebsocketsMessage message)
{
  //Doing something with received String message.data() type
  
  Serial.print("Got Message: ");
  Serial.println(message.data());

}

void setup() 
{
  Serial.begin(9600);

  //s2.attach(D1);
  //s2.write(0);


  while (!Serial && millis() < 5000);
  
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
  bool connected = client.connect(websockets_server_host, websockets_server_port, "/");
  /*
  if (connected) 
  {
    //Serial.println("Connected!");

  } 
  else 
  {
    //Serial.println("Not Connected!");
  }
  */
 //}
}
void loop() 
{
  // let the websockets client check for incoming messages
  if (client.available()) 
  {
    client.poll();
  }
objectDetectionTest();
}
