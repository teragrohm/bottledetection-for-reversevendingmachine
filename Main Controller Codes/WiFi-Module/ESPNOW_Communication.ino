#include <ESP8266WiFi.h>
#include <espnow.h>
#include "FS.h"

int count;
String lastIP;

// Structure example to receive data
// Must match the sender structure
typedef struct struct_message {
    char IP[24];
    //int b;
    bool APshared;
    bool IPtransmission;
    char connected_to[64];
    char validation[16];

} struct_message;

// Create a struct_message called myData
struct_message myData;

//constexpr char WIFI_SSID[] = "September";

int32_t channel;

IPAddress ap_addr(192, 168, 10, 1);
IPAddress gateway;
IPAddress subnet(255, 255, 255, 0);

int32_t getWiFiChannel(const char *ssid) {
  if (int32_t n = WiFi.scanNetworks()) {
      for (uint8_t i=0; i<n; i++) {
          if (!strcmp(ssid, WiFi.SSID(i).c_str())) {
              return WiFi.channel(i);
          }
      }
  }
  return 0;
}

// Callback function that will be executed when data is received
void OnDataRecv(uint8_t * mac, uint8_t *incomingData, uint8_t len) {
  memcpy(&myData, incomingData, sizeof(myData));
  // Serial.print("Bytes received: ");
  // Serial.println(len);

  //Serial.println(myData.validation);

  if (count % 10 == 0) {

 // if (myData.IPtransmission)     
  //{
      Serial.print("CameraIP:");
  //}  
    Serial.print(myData.IP);
    Serial.print('\n');

    count -= (count - 1); 
  }
  else {

    Serial.print(myData.validation);
    Serial.print('\n');
  }

/*
   if (myData.IPtransmission) {
    writeFile(SPIFFS, "/lastIP.txt", myData.IP);
    delay(1000);
  } 
*/
  if (!(myData.APshared)) {

    const char *SSID = myData.connected_to;
    delay(1000);
    writeFile(SPIFFS, "/SSID.txt", SSID);
    delay(500);

    WiFi.softAPdisconnect();
    ESP.restart();
  }
  count++;
  //Serial.print("Count value: ");
  //Serial.println(count);
}

void initFS() {
  SPIFFS.begin();
  /* if (SPIFFS.begin()) {
    Serial.println("File system mounted with success");
  } else {
    Serial.println("Error mounting the file system");
    return;
  } */
}

void writeFile(fs::FS &fs, const char * path, const char * message){
  //Serial.printf("Writing file: %s\r\n", path);

  File file = fs.open(path, "w");
 /* if(!file){
    Serial.println("- failed to open file for writing");
    return;
  } */
  if(file.print(message)){
  /*  Serial.println("- file written");
  } else {
    Serial.println("- write failed");*/
  } 
}

// Read File from SPIFFS or the file system
String readFile(fs::FS &fs, const char * path){
 // Serial.printf("Reading file: %s\r\n", path);

  //File file = SPIFFS.open(path, "r");
  File file = fs.open(path, "r");
/*  if(!file || file.isDirectory()){
    Serial.println("- failed to open file for reading");
    //return String();
  }
*/
  String fileContent;
  while(file.available()){
    fileContent = file.readStringUntil('\n');
    break;
    //Serial.write(file.read());
///    Serial.println(fileContent);
  }
  file.close();
  //return String(file.read());
  fileContent.trim();
  return fileContent;
}
  
void setup() {
  // Initialize Serial Monitor
  Serial.begin(9600);
  pinMode(D4, OUTPUT);
  initFS();
  count = 10;

  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(ap_addr, gateway, subnet);

  String SSID = readFile(SPIFFS, "/SSID.txt");
  //Serial.println(SSID);

  if (SSID.length() > 0) {

    delay(8000);
    channel = getWiFiChannel(SSID.c_str());
    //Serial.println(channel);
    delay(2000);
    WiFi.softAP("RVM WiFi Module", NULL, channel);
  }
  else 
  {
    WiFi.softAP("RVM WiFi Module", NULL);
    delay(3000);
  }

  //Serial.println(WiFi.softAPIP());

  // Set device as a Wi-Fi Station

  //  Serial.println("Setting as a Wi-Fi Station..");
   

  // Serial.print("Station IP Address: ");
  //Serial.println(WiFi.localIP());
  //Serial.print("Wi-Fi Channel: ");
  //Serial.println(WiFi.channel());

  // Init ESP-NOW
  if (esp_now_init() != 0) {
    // Serial.println("Error initializing ESP-NOW");
    return;
  }
  
  // Once ESPNow is successfully Init, we will register for recv CB to
  // get recv packer info
  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
  esp_now_register_recv_cb(OnDataRecv);
}

void loop() {
  //client.loop();
} 