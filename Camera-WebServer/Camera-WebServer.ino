#include <esp_camera.h>
#include <esp_int_wdt.h>
#include <esp_task_wdt.h>
#include <esp_now.h>
#include <esp_wifi.h>
//#include <WiFi.h>
#include <DNSServer.h>
#include <WiFiUdp.h>
//#include <ArduinoOTA.h>
#include "src/parsebytes.h"
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <ArduinoJson.h>
//#include "myconfig.h"

/* This sketch is a extension/expansion/reork of the 'official' ESP32 Camera example
 *  sketch from Expressif:
 *  https://github.com/espressif/arduino-esp32/tree/master/libraries/ESP32/examples/Camera/CamwriteeraWebServer
 *
 *  The web UI has had changes to add the lamp control, rotation, a standalone viewer,
 *  more feeedback, new controls and other tweaks.
 *  
 *
 *  note: Make sure that you select ESP32 AI Thinker,  
 *  or another board which has PSRAM enabled to use high resolution camera modes.
 *  
 *
 *  
 *  
 * 
 */


/*
 *  FOR NETWORK AND HARDWARE SETTINGS COPY OR RENAME 'myconfig.sample.h' TO 'myconfig.h' AND EDIT THAT.
 *
 *  By default this sketch will assume an AI-THINKER ESP-CAM and create
 *  an accesspoint called "ESP32-CAM-CONNECT" (password: "InsecurePassword")
 *
 */

// Search for WiFi configuration parameter in HTTP POST request
const char* PARAM_INPUT_1 = "ssid";
const char* PARAM_INPUT_2 = "pass";
const char* PARAM_INPUT_3 = "ip";
const char* PARAM_INPUT_4 = "gateway";

// Variables to save values from HTML form
String ssid;
String pass;
String ip;
//char ip[32];
String gw;

// File paths to save input values permanently
const char* ssidPath = "/ssid.txt";
const char* passPath = "/pass.txt";
const char* ipPath = "/ip.txt";
const char* gatewayPath = "/gateway.txt";

AsyncWebServer server(100);
AsyncWebSocket ws("/ws");

bool wifi_discovered = false;

// Set your Board ID (ESP32 Sender #1 = BOARD_ID 1, ESP32 Sender #2 = BOARD_ID 2, etc)
#define BOARD_ID 1


//MAC Address of the receiver 
uint8_t broadcastAddress[] = {0x08, 0x3A, 0x8D, 0xCC, 0xD3, 0xE8};

//Structure example to send data
//Must match the receiver structure
typedef struct struct_message {
    char IP[24];
    //int b;
    bool APshared;
    bool IPtransmission;
    char connected_to[64];
    char validation[16];

} struct_message;

esp_now_peer_info_t peerInfo;

//Create a struct_message called myData
struct_message myData;

String esp32_ip;

unsigned long previousMillis = 0;   // Stores last time temperature was published
const long interval = 30000;        // Interval at which to publish sensor readings

// Primary config, or defaults.
#if __has_include("myconfig.h")
    struct station { const char ssid[65]; const char password[65]; const bool dhcp;};  // do no edit
    #include "myconfig.h"
#else
    #warning "Using Defaults: Copy myconfig.sample.h to myconfig.h and edit that to use your own settings"
    #define WIFI_AP_ENABLE
    #define CAMERA_MODEL_AI_THINKER
    struct station { const char ssid[65]; const char password[65]; const bool dhcp;}
    stationList[] = {{"ESP32-CAM-CONNECT","InsecurePassword", true}};
#endif

// Upstream version string
#include "src/version.h"

// Pin Mappings
#include "camera_pins.h"

// Camera config structure
camera_config_t config;

// Internal filesystem (SPIFFS)
// used for non-volatile camera settings
#include "storage.h"

// Sketch Info
int sketchSize;
int sketchSpace;
String sketchMD5;

// Start with accesspoint mode disabled, wifi setup will activate it if
// no known networks are found, and WIFI_AP_ENABLE has been defined
bool accesspoint = false;

// IP address, Netmask and Gateway, populated when connected
IPAddress ip_addr;
IPAddress local_IP(192, 168, 4, 100);
IPAddress gateway;
IPAddress subnet(255, 255, 0, 0);

// Declare external function from app_httpd.cpp
extern void startCameraServer(int hPort, int sPort);
extern void serialDump();

// Names for the Camera. (set these in myconfig.h)
#if defined(CAM_NAME)
    char myName[] = CAM_NAME;
#else
    char myName[] = "ESP32 camera server";
#endif

#if defined(MDNS_NAME)
    char mdnsName[] = MDNS_NAME;
#else
    char mdnsName[] = "esp32-cam";
#endif

// Ports for http and stream (override in myconfig.h)
#if defined(HTTP_PORT)
    int httpPort = HTTP_PORT;
#else
    int httpPort = 80;
#endif

#if defined(STREAM_PORT)
    int streamPort = STREAM_PORT;
#else
    int streamPort = 81;
#endif

#if !defined(WIFI_WATCHDOG)
    #define WIFI_WATCHDOG 15000
#endif

// Number of known networks in stationList[]
int stationCount = sizeof(stationList)/sizeof(stationList[0]);

// If we have AP mode enabled, ignore first entry in the stationList[]
#if defined(WIFI_AP_ENABLE)
    int firstStation = 1;
#else
    int firstStation = 0;
#endif

// Select between full and simple index as the default.
#if defined(DEFAULT_INDEX_FULL)
    char default_index[] = "full";
#else
    char default_index[] = "simple";
#endif

// DNS server
const byte DNS_PORT = 53;
DNSServer dnsServer;
bool captivePortal = false;
char apName[64] = "Undefined";

// The app and stream URLs
char httpURL[64] = {"Undefined"};
char streamURL[64] = {"Undefined"};

// Counters for info screens and debug
int8_t streamCount = 0;          // Number of currently active streams
unsigned long streamsServed = 0; // Total completed streams
unsigned long imagesServed = 0;  // Total image requests

// This will be displayed to identify the firmware
char myVer[] PROGMEM = __DATE__ " @ " __TIME__;

// This will be set to the sensors PID (identifier) during initialisation
//camera_pid_t sensorPID;
int sensorPID;

// Camera module bus communications frequency.
// Originally: config.xclk_freq_mhz = 20000000, but this lead to visual artifacts on many modules.
// See https://github.com/espressif/esp32-camera/issues/150#issuecomment-726473652 et al.
#if !defined (XCLK_FREQ_MHZ)
    unsigned long xclk = 8;
#else
    unsigned long xclk = XCLK_FREQ_MHZ;
#endif

// initial rotation
// can be set in myconfig.h
#if !defined(CAM_ROTATION)
    #define CAM_ROTATION 0
#endif
int myRotation = CAM_ROTATION;

// minimal frame duration in ms, effectively 1/maxFPS
#if !defined(MIN_FRAME_TIME)
    #define MIN_FRAME_TIME 0
#endif
int minFrameTime = MIN_FRAME_TIME;

// Illumination LAMP and status LED
#if defined(LAMP_DISABLE)
    int lampVal = -1; // lamp is disabled in config
#elif defined(LAMP_PIN)
    #if defined(LAMP_DEFAULT)
        int lampVal = constrain(LAMP_DEFAULT,0,100); // initial lamp value, range 0-100
    #else
        int lampVal = 0; //default to off
    #endif
#else
    int lampVal = -1; // no lamp pin assigned
#endif

#if defined(LED_DISABLE)
    #undef LED_PIN    // undefining this disables the notification LED
#endif

bool autoLamp = false;         // Automatic lamp (auto on while camera running)

int lampChannel = 7;           // a free PWM channel (some channels used by camera)
const int pwmfreq = 50000;     // 50K pwm frequency
const int pwmresolution = 9;   // duty cycle bit range
const int pwmMax = pow(2,pwmresolution)-1;

#if defined(NO_FS)
    bool filesystem = false;
#else
    bool filesystem = true;
#endif

//#if defined(NO_OTA)
// bool otaEnabled = false;
//#else
//    bool otaEnabled = true;
//#endif
/*
#if defined(OTA_PASSWORD)
    char otaPassword[] = OTA_PASSWORD;
#else
    char otaPassword[] = "";
#endif
*/
/*
#if defined(NTPSERVER)
    bool haveTime = true;
    const char* ntpServer = NTPSERVER;
    const long  gmtOffset_sec = NTP_GMT_OFFSET;
    const int   daylightOffset_sec = NTP_DST_OFFSET;
#else
    bool haveTime = false;
    const char* ntpServer = "";
    const long  gmtOffset_sec = 0;
    const int   daylightOffset_sec = 0;
#endif
*/
// Critical error string; if set during init (camera hardware failure) it
// will be returned for all http requests
String critERR = "";

// Debug flag for stream and capture data
bool debugData;

void debugOn() {
    debugData = true;
    Serial.println("Camera debug data is enabled (send 'd' for status dump, or any other char to disable debug)");
}

void debugOff() {
    debugData = false;
    Serial.println("Camera debug data is disabled (send 'd' for status dump, or any other char to enable debug)");
}

// Serial input (debugging controls)
void handleSerial() {
    if (Serial.available()) {
        char cmd = Serial.read();
        if (cmd == 'd' ) {
            serialDump();
        } else {
            if (debugData) debugOff();
            else debugOn();
        }
    }
    while (Serial.available()) Serial.read();  // chomp the buffer
}

// Notification LED
void flashLED(int flashtime) {
#if defined(LED_PIN)                // If we have it; flash it.
    digitalWrite(LED_PIN, LED_ON);  // On at full power.
    delay(flashtime);               // delay
    digitalWrite(LED_PIN, LED_OFF); // turn Off
#else
    return;                         // No notifcation LED, do nothing, no delay
#endif
}

// Lamp Control
void setLamp(int newVal) {
#if defined(LAMP_PIN)
    if (newVal != -1) {
        // Apply a logarithmic function to the scale.
        int brightness = round((pow(2,(1+(newVal*0.02)))-2)/6*pwmMax);
        ledcWrite(lampChannel, brightness);
        Serial.print("Lamp: ");
        Serial.print(newVal);
        Serial.print("%, pwm = ");
        Serial.println(brightness);
    }
#endif
}

void calcURLs() {
    // Set the URL's

    ip_addr = WiFi.localIP();
    
    #if defined(URL_HOSTNAME)
        if (httpPort != 80) {
            sprintf(httpURL, "http://%s:%d/", URL_HOSTNAME, httpPort);
        } else {
            sprintf(httpURL, "http://%s/", URL_HOSTNAME);
        }
        sprintf(streamURL, "http://%s:%d/", URL_HOSTNAME, streamPort);
    #else
        Serial.println("Setting httpURL");
        if (httpPort != 80) {
            sprintf(httpURL, "http://%d.%d.%d.%d:%d/", ip, ip[1], ip[2], ip[3], httpPort);
        } else {
            sprintf(httpURL, "http://%d.%d.%d.%d/", ip_addr[0], ip_addr[1], ip_addr[2], ip_addr[3]);
        }
        sprintf(streamURL, "http://%d.%d.%d.%d:%d/", ip_addr[0], ip_addr[1], ip_addr[2], ip_addr[3], streamPort);
        Serial.println(ip_addr);
        //sprintf(ip, "http://%d.%d.%d.%d:%d/", ip[0], ip[1], ip[2], ip[3], streamPort);
    #endif
}

void StartCamera() {
    // Populate camera config structure with hardware and other defaults
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = xclk * 1000000;
    config.pixel_format = PIXFORMAT_JPEG;
    // Low(ish) default framesize and quality
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.fb_count = 2;
    config.grab_mode = CAMERA_GRAB_LATEST;

    #if defined(CAMERA_MODEL_ESP_EYE)
        pinMode(13, INPUT_PULLUP);
        pinMode(14, INPUT_PULLUP);
    #endif

    // camera init
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        delay(100);  // need a delay here or the next serial o/p gets missed
        Serial.printf("\r\n\r\nCRITICAL FAILURE: Camera sensor failed to initialise.\r\n\r\n");
        Serial.printf("A full (hard, power off/on) reboot will probably be needed to recover from this.\r\n");
        Serial.printf("Meanwhile; this unit will reboot in 1 minute since these errors sometime clear automatically\r\n");
        // Reset the I2C bus.. may help when rebooting.
        periph_module_disable(PERIPH_I2C0_MODULE); // try to shut I2C down properly in case that is the problem
        periph_module_disable(PERIPH_I2C1_MODULE);
        periph_module_reset(PERIPH_I2C0_MODULE);
        periph_module_reset(PERIPH_I2C1_MODULE);
        // And set the error text for the UI
        critERR = "<h1>Error!</h1><hr><p>Camera module failed to initialise!</p><p>Please reset (power off/on) the camera.</p>";
        critERR += "<p>We will continue to reboot once per minute since this error sometimes clears automatically.</p>";
        // Start a 60 second watchdog timer
        esp_task_wdt_init(60,true);
        esp_task_wdt_add(NULL);
    } else {
        Serial.println("Camera init succeeded");

        // Get a reference to the sensor
        sensor_t * s = esp_camera_sensor_get();

        // Dump camera module, warn for unsupported modules.
        sensorPID = s->id.PID;
        switch (sensorPID) {
            case OV9650_PID: Serial.println("WARNING: OV9650 camera module is not properly supported, will fallback to OV2640 operation"); break;
            case OV7725_PID: Serial.println("WARNING: OV7725 camera module is not properly supported, will fallback to OV2640 operation"); break;
            case OV2640_PID: Serial.println("OV2640 camera module detected"); break;
            case OV3660_PID: Serial.println("OV3660 camera module detected"); break;
            default: Serial.println("WARNING: Camera module is unknown and not properly supported, will fallback to OV2640 operation");
        }

        // OV3660 initial sensors are flipped vertically and colors are a bit saturated
        if (sensorPID == OV3660_PID) {
            s->set_vflip(s, 1);  // flip it back
            s->set_brightness(s, 1);  // up the brightness just a bit
            s->set_saturation(s, -2);  // lower the saturation
        }
        /*
        // M5 Stack Wide has special needs
        #if defined(CAMERA_MODEL_M5STACK_WIDE)
            s->set_vflip(s, 1);
            s->set_hmirror(s, 1);
        #endif
        */
        // Config can override mirror and flip
        #if defined(H_MIRROR)
            s->set_hmirror(s, H_MIRROR);
        #endif
        #if defined(V_FLIP)
            s->set_vflip(s, V_FLIP);
        #endif

        // set initial frame rate
        #if defined(DEFAULT_RESOLUTION)
            s->set_framesize(s, DEFAULT_RESOLUTION);
        #endif

        /*
        * Add any other defaults you want to apply at startup here:
        * uncomment the line and set the value as desired (see the comments)
        *
        * these are defined in the esp headers here:
        * https://github.com/espressif/esp32-camera/blob/master/driver/include/sensor.h#L149
        */

        //s->set_framesize(s, FRAMESIZE_SVGA); // FRAMESIZE_[QQVGA|HQVGA|QVGA|CIF|VGA|SVGA|XGA|SXGA|UXGA|QXGA(ov3660)]);
        //s->set_quality(s, val);       // 10 to 63
        //s->set_brightness(s, 0);      // -2 to 2
        //s->set_contrast(s, 0);        // -2 to 2
        //s->set_saturation(s, 0);      // -2 to 2
        //s->set_special_effect(s, 0);  // 0 to 6 (0 - No Effect, 1 - Negative, 2 - Grayscale, 3 - Red Tint, 4 - Green Tint, 5 - Blue Tint, 6 - Sepia)
        //s->set_exposure_ctrl(s, 1);   // 0 = disable , 1 = enable
        //s->set_raw_gma(s, 1);         // 0 = disable , 1 = enable
        //s->set_lenc(s, 1);            // 0 = disable , 1 = enable
        //s->set_hmirror(s, 0);         // 0 = disable , 1 = enable
        //s->set_vflip(s, 0);           // 0 = disable , 1 = enable
        //s->set_dcw(s, 1);             // 0 = disable , 1 = enable
        //s->set_colorbar(s, 0);        // 0 = disable , 1 = enable
    }
    // We now have camera with default init
}

void writeFile(fs::FS &fs, const char * path, const char * message){
  Serial.printf("Writing file: %s\r\n", path);

  File file = fs.open(path, FILE_WRITE);
  if(!file){
    Serial.println("- failed to open file for writing");
    return;
  }
  if(file.print(message)){
    Serial.println("- file written");
  } else {
    Serial.println("- write failed");
  }
}

// Read File from SPIFFS or the file system
String readFile(fs::FS &fs, const char * path){
  Serial.printf("Reading file: %s\r\n", path);

  File file = SPIFFS.open(path, "r");
  if(!file || file.isDirectory()){
    Serial.println("- failed to open file for reading");
    //return String();
  }

  String fileContent;
  while(file.available()){
    fileContent = file.readStringUntil('\n');
    break;
    //Serial.write(file.read());
    Serial.println(fileContent);
  }
  file.close();
  //return String(file.read());
  fileContent.trim();
  return fileContent;
}

class CaptiveRequestHandler : public AsyncWebHandler {
public:
  CaptiveRequestHandler() {}
  virtual ~CaptiveRequestHandler() {}

  bool canHandle(AsyncWebServerRequest *request){
    //request->addInterestingHeader("ANY");
    return true;
  }

  void handleRequest(AsyncWebServerRequest *request) {
    //request->send_P(200, "text/html", index_html);
    request->send(SPIFFS, "/wifimanager.html", "text/html"); 
  }
};

void WifiSetup() {
    // Feedback that we are now attempting to connect
    flashLED(300);
    delay(100);
    flashLED(300);
    Serial.println("Starting WiFi");

    // Disable power saving on WiFi to improve responsiveness
    // (https://github.com/espressif/arduino-esp32/issues/1484)
        WiFi.setSleep(false);
    
        WiFi.setHostname(mdnsName);

        // Initiate network connection request 
        
         //ssid = readFile(SPIFFS, ssidPath);
         ssid = "realme 5i";
         pass = readFile(SPIFFS, passPath);

       /* if (!WiFi.config(ip_addr, gateway, subnet)) 
       {
          Serial.println("STA Failed to configure");
       }
       else 
       {*/
         //WiFi.begin(ssid.c_str(), pass.c_str());
         WiFi.begin("realme 5i", "12345678");
       //}

        // Wait to connect, or timeout
        unsigned long start = millis();
        while ((millis() - start <= 5000) && (WiFi.status() != WL_CONNECTED)) {
            delay(500);
            Serial.print('.');
        }

        //IPAddress IP = WiFi.localIP();
        //Serial.print("IP address: ");
        //Serial.println(IP); 


        calcURLs();

  }



void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){

  if(type == WS_EVT_CONNECT){

    Serial.println("Websocket client connection received");
    ws.textAll("You are now connected.");

  } else if(type == WS_EVT_DISCONNECT){
    Serial.println("Client disconnected");

  } else if(type == WS_EVT_DATA){

    Serial.println("Data received: ");

    for(int i=0; i < len; i++) {
          Serial.print((char) data[i]);
    }
    ws.closeAll();

    int chArr_length = strlen(myData.validation);

     for (int i = 0; i < chArr_length; i++) {
          Serial.print(myData.validation[i]);
          myData.validation[i] = '\0';
     }
    
    Serial.println();
    objectDetected(arg, data, len);
    while (millis() - millis() <= 20000) {
            Serial.print('.');
        }

  }
}

void objectDetected(void *arg, uint8_t *data, size_t len) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {

        const uint8_t size = JSON_OBJECT_SIZE(1);
        StaticJsonDocument<size> json;
        DeserializationError err = deserializeJson(json, data);
        if (err) {
            Serial.print(F("deserializeJson() failed with code "));
            Serial.println(err.c_str());
            return;
        }

        //const char *detectionResult = json["object"];
        String detectionResult = json["object"];
        
        //if (strcmp(detectionResult, "bottle") == 0) {
        if (detectionResult.indexOf("bottle") >= 0) {
          ws.textAll("A bottle was detected.");
          //ws.textAll("Valid");
          strcat(myData.validation, "Valid");
        }
        
        //else if (strcmp(detectionResult, "bottle") != 0) {
        else if (detectionResult.indexOf("bottle") < 0) {
          //ws.textAll("Invalid");
          strcat(myData.validation, "Invalid");
        }
        //vTaskDelay(pdMS_TO_TICKS(2000));
        
        Serial.println(strlen(myData.validation));
        esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));

    }
}

// callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");

  
  //if (!(WiFi.SSID() == ssid) || wifi_discovered) 
  if (wifi_discovered) 
  {
    WifiSetup();
    startCameraServer(httpPort, streamPort); 
   /* unsigned long start = millis();
    WiFi.begin(ssid.c_str(), pass.c_str());
    Serial.print("Reconnecting");

    while ((millis() - start <= 8000) && (WiFi.status() != WL_CONNECTED)) {
            delay(500);
            Serial.print('.');

            if (WiFi.status() == WL_CONNECTED) 
            {
              calcURLs();
              break;
            } }*/
        } 


    wifi_discovered = false;
    
    delay(2000);
    //wifi_discovered = false; 
  //else Serial.println("Delivery Failed"); 
}

void setup() {
    Serial.begin(115200);
    Serial.setDebugOutput(true);
    Serial.println();
    Serial.println("====");
    Serial.print("esp32-cam-webserver: ");
    Serial.println(myName);
    Serial.print("Code Built: ");
    Serial.println(myVer);
    Serial.print("Base Release: ");
    Serial.println(baseVersion);
    Serial.println();

    // Warn if no PSRAM is detected (typically user error with board selection in the IDE)
    if(!psramFound()){
        Serial.println("\r\nFatal Error; Halting");
        while (true) {
            Serial.println("No PSRAM found; camera cannot be initialised: Please check the board config for your module.");
            delay(5000);
        }
    }

    if (stationCount == 0) {
        Serial.println("\r\nFatal Error; Halting");
        while (true) {
            Serial.println("No wifi details have been configured; we cannot connect to existing WiFi or start our own AccessPoint, there is no point in proceeding.");
            delay(5000);
        }
    }

    #if defined(LED_PIN)  // If we have a notification LED, set it to output
        pinMode(LED_PIN, OUTPUT);
        digitalWrite(LED_PIN, LED_ON);
    #endif

    // Start the SPIFFS filesystem before we initialise the camera
    if (filesystem) {
        filesystemStart();
        delay(200); // a short delay to let spi bus settle after SPIFFS init
    }

    // Start (init) the camera 
    StartCamera();

    // Now load and apply any saved preferences
    if (filesystem) {
        delay(200); // a short delay to let spi bus settle after camera init
        loadPrefs(SPIFFS);
    } else {
        Serial.println("No Internal Filesystem, cannot load or save preferences");
    }

    /*
    * Camera setup complete; initialise the rest of the hardware.
    */

    // Start Wifi and loop until we are connected or have started an AccessPoint
    //while ((WiFi.status() != WL_CONNECTED) && !accesspoint)  {
    while (WiFi.status() != WL_CONNECTED) {


       /*if (!WiFi.config(local_IP, gateway, subnet)) 
       {
          Serial.println("STA Failed to configure");
       }
       else
       {
           WiFi.config(local_IP, gateway, subnet);*/
           //WiFi.begin("RVM WiFi Module");
           WiFi.begin("realme 5i", ("12345678"));
       //}

        // Wait to connect, or timeout
        unsigned long start = millis();
        while ((millis() - start <= 5000) && (WiFi.status() != WL_CONNECTED)) {
            delay(500);
            Serial.print('.');
        }

        Serial.print("AP IP address: ");
        Serial.println(WiFi.localIP()); 
    }

    // Init ESP-NOW
      if (esp_now_init() != ESP_OK) {
      Serial.println("Error initializing ESP-NOW");
      return;
    }

      // Once ESPNow is successfully Init, we will register for Send CB to
      // get the status of Trasnmitted packet
      esp_now_register_send_cb(OnDataSent);
  
      // Register peer
      memcpy(peerInfo.peer_addr, broadcastAddress, 6);
      peerInfo.encrypt = false;
  
      // Add peer        
      if (esp_now_add_peer(&peerInfo) != ESP_OK){
      Serial.println("Failed to add peer");
      return;
    }

        Serial.println("OTA is disabled");

        if (!MDNS.begin(mdnsName)) {
          Serial.println("Error setting up MDNS responder!");
        }
        Serial.println("mDNS responder started");
    //}

    //MDNS Config -- note that if OTA is NOT enabled this needs prior steps!
    MDNS.addService("http", "tcp", 80);
    Serial.println("Added HTTP service to MDNS server");

    // Gather static values used when dumping status; these are slow functions, so just do them once during startup
    //sketchSize = ESP.getSketchSize();
    //sketchSpace = ESP.getFreeSketchSpace();
    //sketchMD5 = ESP.getSketchMD5();

    // Initialise and set the lamp
    if (lampVal != -1) {
        #if defined(LAMP_PIN)
            ledcSetup(lampChannel, pwmfreq, pwmresolution);  // configure LED PWM channel
            ledcAttachPin(LAMP_PIN, lampChannel);            // attach the GPIO pin to the channel
            if (autoLamp) setLamp(0);                        // set default value
            else setLamp(lampVal);
         #endif
    } else {
        Serial.println("No lamp, or lamp disabled in config");
    }

    // Start the camera server
    startCameraServer(httpPort, streamPort);

    if (critERR.length() == 0) {
        Serial.printf("\r\nCamera Ready!\r\nUse '%s' to connect\r\n", httpURL);
        Serial.printf("Stream viewer available at '%sview'\r\n", streamURL);
        Serial.printf("Raw stream URL is '%s'\r\n", streamURL);
        #if defined(DEBUG_DEFAULT_ON)
            debugOn();
        #else
            debugOff();
        #endif
    } else {
        Serial.printf("\r\nCamera unavailable due to initialisation errors.\r\n\r\n");
    }

    // Info line; use for Info messages; eg 'This is a Beta!' warnings, etc. as necesscary
    // Serial.print("\r\nThis is the 4.1 beta\r\n");

    // As a final init step chomp out the serial buffer in case we have recieved mis-keys or garbage during startup
    while (Serial.available()) Serial.read();

    // Web Server Root URL
        server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(SPIFFS, "/wifimanager.html", "text/html");
    });
    
    server.serveStatic("/", SPIFFS, "/");
    
    server.on("/", HTTP_POST, [](AsyncWebServerRequest *request) {
      int params = request->params();  
      for(int i=0;i<params;i++){
        const AsyncWebParameter* p = request->getParam(i);
        if(p->isPost()){
          // HTTP POST ssid value
          if (p->name() == PARAM_INPUT_1) {
            ssid = p->value().c_str();
            Serial.print("SSID set to: ");
            Serial.println(ssid);
            // Write file to save value
            writeFile(SPIFFS, ssidPath, ssid.c_str());
          }
          // HTTP POST pass value
          if (p->name() == PARAM_INPUT_2) {
            pass = p->value().c_str();
            Serial.print("Password set to: ");
            Serial.println(pass);
            // Write file to save value
            writeFile(SPIFFS, passPath, pass.c_str());
          }
          // HTTP POST ip value
          if (p->name() == PARAM_INPUT_3) {
            ip = p->value().c_str();
           //ip = p->value();
            Serial.print("IP Address set to: ");
            Serial.println(ip);
            // Write file to save value
            writeFile(SPIFFS, ipPath, ip.c_str());
          }
          // HTTP POST gateway value
          if (p->name() == PARAM_INPUT_4) {
            gw = p->value().c_str();
            Serial.print("Gateway set to: ");
            Serial.println(gw);
            // Write file to save value
            writeFile(SPIFFS, gatewayPath, gw.c_str());
          }
          //Serial.printf("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
          
        }
      }
      request->send(200, "text/plain", "Done. ESP will restart, connect to same WiFi network and go to IP address: " + ip);
      
      myData.APshared = false;
      ssid.toCharArray(myData.connected_to, ssid.length() + 1);
      wifi_discovered = true;
      esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
    });
    dnsServer.start(100, "*", WiFi.localIP());
    server.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER);//only when requested from AP      

    ws.onEvent(onWsEvent);
    server.addHandler(&ws);

    server.begin();

  //ssid = readFile(SPIFFS, ssidPath);
  ssid = "realme 5i";
  pass = readFile(SPIFFS, passPath);
  
  /* esp32_ip = WiFi.localIP().toString();
  Serial.println(esp32_ip);
   
  esp32_ip.toCharArray(myData.IP, esp32_ip.length() + 1); */
  //Serial.println(myData.IP);
  myData.APshared = false;
  //myData.IPtransmission = true;

  if (ssid.length() > 0) 
  {  
    ssid.toCharArray(myData.connected_to, ssid.length() + 1);
    Serial.println(myData.connected_to);
  }
  
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
  Serial.println(result == ESP_OK ? "ESP32CAM IP was sent" : "Attempt to send ESP32CAM IP failed");
}

void loop() {
    /*
     *  Just loop forever, reconnecting Wifi As necesscary in client mode
     * The stream and URI handler processes initiated by the startCameraServer() call at the
     * end of setup() will handle the camera and UI processing from now on.
    */

    unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    // Save the last time a new reading was published
    previousMillis = currentMillis;
    
    myData.APshared = true;

if (WiFi.status() != WL_CONNECTED || !(WiFi.SSID() == ssid)) {

    if (WiFi.status() != WL_CONNECTED)
    { 
      //WiFi.begin("RVM WiFi Module");
      WiFi.begin("realme 5i", "12345678");

      Serial.print("To access the WiFi manager, go to this URL on your browser: http://");
      Serial.println(WiFi.localIP());
      delay(15000); 
      //myData.APshared = false;
      //ESP.restart();
    }
    else if(!(WiFi.SSID() == ssid)) {
      
      //WiFi.begin(ssid.c_str(), pass.c_str());
      WifiSetup();
      startCameraServer(httpPort, streamPort);
      delay(5000);

      esp32_ip = WiFi.localIP().toString();
      Serial.println(esp32_ip);
      esp32_ip.toCharArray(myData.IP, esp32_ip.length() + 1);
  
      myData.APshared = false;
      myData.IPtransmission = true;

      if (ssid.length() > 0) 
      {  
        ssid.toCharArray(myData.connected_to, ssid.length() + 1);
        Serial.println(myData.connected_to);
      }
      esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
    }
    //calcURLs();
     
}
    //Set values to send
    esp32_ip = WiFi.localIP().toString();
    esp32_ip.toCharArray(myData.IP, esp32_ip.length() + 1);
    Serial.println(myData.IP);

     
    //Send message via ESP-NOW
    //esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
    /* if (result == ESP_OK) {
      Serial.println("Sent with success");
    }
    else {
      Serial.println("Error sending the data");
    }
    */
  }

}
