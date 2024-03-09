// ESP32 Board manager version 2.0.8
// Arduino IDE: Board = AIThinker ESP32-CAM, XIAO_ESP32S3, ...
// Browser softAP: http://z.be or http://192.168.4.1


#include <ArduinoWebsockets.h>
#include <ESPAsyncWebSrv.h> 

#include <WiFi.h>
#include <driver/ledc.h>

#define USE_CAMERA 1
#define DEBUG_SERIAL Serial

#ifdef USE_CAMERA
#include "esp_camera.h"
#endif

const char* ssid = "SignIn426-FiberToHome"; //Enter SSID
const char* password = "aa112233"; //Enter Password
// #define USE_SOFTAP
// #define WIFI_SOFTAP_CHANNEL 1 // 1-13
// const char ssid[] = "BlimpCam-";
// const char password[] = "12345678";

#ifdef USE_SOFTAP
#include <DNSServer.h>
DNSServer dnsServer;
#else
#include <ESPmDNS.h>
#endif

#include "zeppelincam_html.h" // Do not put html code in ino file to avoid problems with the preprocessor

#ifdef ARDUINO_XIAO_ESP32S3
#define CAMERA_MODEL_XIAO_ESP32S3 // Has PSRAM

// const int fwdPin = D0;  //Forward Motor Pin
// const int turnPin = D10;  //Steering Servo Pin
// const int upPin = D1;  // Up Pin
// const int hbridgePinA = D4; // H-bridge pin A
// const int hbridgePinB = D5; // H-bridge pin B

#define PIN_LED_DIGIT LED_BUILTIN
#define LED_ON LOW
#define LED_OFF HIGH

// end ARDUINO_XIAO_ESP32S3

#elif defined(ARDUINO_LOLIN_C3_MINI)
// Uncomment USE_CAMERA
// const int fwdPin = 2;  //Forward Motor Pin
// const int turnPin = 1;  //Steering Servo Pin
// const int upPin = 5;  // Up Pin
// const int hbridgePinA = 3; // H-bridge pin A
// const int hbridgePinB = 4; // H-bridge pin B

#define PIN_LED_DIGIT 6
#define LED_ON HIGH
#define LED_OFF LOW

#else // not ARDUINO_XIAO_ESP32S3, not ARDUINO_LOLIN_C3_MINI but another ESP32
// AI-Thinker ESP32-CAM pin definitions

#define CAMERA_MODEL_AI_THINKER // Has PSRAM
//#define CAMERA_MODEL_XIAO_ESP32S3 // Has PSRAM

//#define CAMERA_MODEL_WROVER_KIT // Has PSRAM
//#define CAMERA_MODEL_ESP_EYE // Has PSRAM
//#define CAMERA_MODEL_ESP32S3_EYE // Has PSRAM
//#define CAMERA_MODEL_M5STACK_PSRAM // Has PSRAM
//#define CAMERA_MODEL_M5STACK_V2_PSRAM // M5Camera version B Has PSRAM
//#define CAMERA_MODEL_M5STACK_WIDE // Has PSRAM
//#define CAMERA_MODEL_M5STACK_ESP32CAM // No PSRAM
//#define CAMERA_MODEL_M5STACK_UNITCAM // No PSRAM
//#define CAMERA_MODEL_TTGO_T_JOURNAL // No PSRAM



#define PIN_LED_PWM 4

//#define PIN_LED_DIGIT 4
//#define LED_ON HIGH
//#define LED_OFF LOW

#endif // ARDUINO_XIAO_ESP32S3

#ifdef USE_CAMERA
boolean camera_initialised = false;
int videoswitch = 0;

// video streaming setting
#define MIN_TIME_PER_FRAME 20 // minimum time between video frames in ms e.g. minimum 200ms means max 5fps

#include "camera_pins.h"

camera_fb_t * fb = NULL;
#endif // USE_CAMERA

using namespace websockets;
WebsocketsServer server;
AsyncWebServer webserver(80);
WebsocketsClient sclient;


// timeoutes
#define TIMEOUT_MS_MOTORS 500L // Safety shutdown: motors will go to power off position after x milliseconds no message received
#define TIMEOUT_MS_LED 2L       // LED will light up for x milliseconds after message received

long last_activity_message;

#define MOTOR_TIME_UP 300 // ms to go to ease to full power of a motor 
#define SERVO_SWEEP_TIME 300 // in ms


bool motors_halt;

#define ANALOGWRITE_FREQUENCY 5000
#define ANALOGWRITE_RESOLUTION LEDC_TIMER_12_BIT // LEDC_TIMER_8_BIT
#define PWMRANGE ((1<<ANALOGWRITE_RESOLUTION)-1) // LEDC_TIMER_12_BIT=> 4095; ANALOGWRITE_RANGE LEDC_TIMER_8_BIT => 255

#define LED_BRIGHTNESS_NO_CONNECTION  5
#define LED_BRIGHTNESS_HANDLEMESSAGE  0
#define LED_BRIGHTNESS_BOOT          50
#define LED_BRIGHTNESS_CONNECTED      10
#define LED_BRIGHTNESS_OFF            0

// Channel & timer allocation table

float jx = 0;
float jy = 0;
bool ccwbtn = 0;
bool cwbtn =0;
float s1 =0;

// const int sPin = 2;  // 16 corresponds to GPIO16
// const int JxPin = 12; // 17 corresponds to GPIO17
// const int JyPin = 13;
// const int CwPin = 14;
// const int CcwPin = 15;

// const int freq = 5000;
// const int ledChannel = 0;
// const int resolution = 8;



// LEDC_TIMER_0, channels 0, 1, 8, 9
// Used for H-bridge
// #define CHANNEL_ANALOGWRITE_HBRIDGEA LEDC_CHANNEL_0 // h-bridge pin A
// #define CHANNEL_ANALOGWRITE_HBRIDGEB LEDC_CHANNEL_1 // h-bridge pin B

// // LEDC_TIMER_1, channels 2, 3, 10, 11
// // Used by analogwrite: Forward & Up motor PWM)
// #define CHANNEL_ANALOGWRITE_FORWARD LEDC_CHANNEL_2  // Forward
// #define CHANNEL_ANALOGWRITE_UP      LEDC_CHANNEL_3  // Up motor

#ifdef PIN_LED_PWM
#define CHANNEL_ANALOGWRITE_LED  10 // LED BUILTIN, channel 10 only available on ESP32, not C3/S3/...
#endif

// Servo's: LEDC_TIMER_2, channels 4, 5, 12, 13
// Used by servo
// #define CHANNEL_SERVO2   LEDC_CHANNEL_5

// LEDC_TIMER_3, channels 6, 7, 14, 15. Timer not available on ESP32C3
// Used by camera
#define TIMER_CAMERA LEDC_TIMER_3
#define CHANNEL_CAMERA   LEDC_CHANNEL_6

// ESP32 analogwrite functions
// void analogwrite_attach(uint8_t pin, ledc_channel_t channel)
void analogwrite_attach(uint8_t pin, int channel)
{
  ledcSetup(channel, ANALOGWRITE_FREQUENCY, ANALOGWRITE_RESOLUTION); //channel, freq, resolution
  ledcAttachPin(pin, channel); // pin, channel
#ifdef DEBUG_SERIAL
  // DEBUG_SERIAL.print(F("AnalogWrite: PWMRANGE="));
  // DEBUG_SERIAL.println(PWMRANGE);
#endif
}

// Arduino like analogWrite
// void analogwrite_channel(ledc_channel_t channel, uint32_t value) {
void analogwrite_channel(int channel, uint32_t value) {
  ledcWrite(channel, value);
}

// ESP32 Servo functions


void camera_init()
{
#ifdef USE_CAMERA
  camera_config_t config;
  config.ledc_channel = CHANNEL_CAMERA;
  config.ledc_timer = TIMER_CAMERA;
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
#ifdef ARDUINO_XIAO_ESP32S3
  config.xclk_freq_hz = 20000000;
#else
  config.xclk_freq_hz = 8000000;
#endif
  config.pixel_format = PIXFORMAT_JPEG; // for streaming

  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  if (psramFound()) {
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 9;
    config.fb_count = 2;
    // TODO #if Arduino ESP32 board version: arduino 1.0.6,2.0.0 not include, >=2.0.1 include fb_location & grab_mode
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.grab_mode = CAMERA_GRAB_LATEST;
  } else {
    // Limit the frame size when PSRAM is not available
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 10;
    config.fb_count = 1;
    // TODO #if Arduino ESP32 board version: arduino 1.0.6,2.0.0 not include, >=2.0.1 include fb_location & grab_mode
    config.fb_location = CAMERA_FB_IN_DRAM;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  }

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
#ifdef DEBUG_SERIAL
    DEBUG_SERIAL.printf("Camera init failed with error 0x%x", err);
#endif
    return;
  }
  else
  {
#ifdef DEBUG_SERIAL
    DEBUG_SERIAL.println("Camera initialised correctly.");
#endif
    camera_initialised = true;
  }

  sensor_t * s = esp_camera_sensor_get();
  s->set_framesize(s, FRAMESIZE_SVGA);
#endif
}

void led_init()
{
#ifdef PIN_LED_PWM
  analogwrite_attach(PIN_LED_PWM, CHANNEL_ANALOGWRITE_LED); // pin, channel
#endif
#ifdef PIN_LED_DIGIT
  pinMode(PIN_LED_DIGIT, OUTPUT);
#endif
}

void led_set(int ledmode)
{
#ifdef PIN_LED_PWM
  analogwrite_channel(CHANNEL_ANALOGWRITE_LED, ledmode);
#endif
#ifdef PIN_LED_DIGIT
  digitalWrite(PIN_LED_DIGIT, ledmode == LED_BRIGHTNESS_OFF ? LED_OFF : LED_ON);
#endif
}

void setup()
{
  delay(200);
#ifdef DEBUG_SERIAL
  DEBUG_SERIAL.begin(19200);
  DEBUG_SERIAL.println(F("\nZeppelinCAM started"));
#endif
  // ledcSetup(0, ANALOGWRITE_FREQUENCY, ANALOGWRITE_RESOLUTION);
  // ledcSetup(1, ANALOGWRITE_FREQUENCY, ANALOGWRITE_RESOLUTION);
  // ledcSetup(2, ANALOGWRITE_FREQUENCY, ANALOGWRITE_RESOLUTION);
  // ledcSetup(3, ANALOGWRITE_FREQUENCY, ANALOGWRITE_RESOLUTION);
  // ledcSetup(4, ANALOGWRITE_FREQUENCY, ANALOGWRITE_RESOLUTION);
  
  // attach the channel to the GPIO to be controlled
  // ledcAttachPin(sPin, 0);
  // ledcAttachPin(JxPin, 1);
  // ledcAttachPin(JyPin, 2);
  // ledcAttachPin(CwPin, 3);
  // ledcAttachPin(CcwPin, 4);
  // forward motor PWM
  
  led_init();
  // flash 2 time to show we are rebooting
  led_set(LED_BRIGHTNESS_BOOT);
  delay(10);
  led_set(LED_BRIGHTNESS_OFF);
  delay(100);
  led_set(LED_BRIGHTNESS_BOOT);
  delay(10);
  led_set(LED_BRIGHTNESS_OFF);

  // steering servo PWM
  
  // Don't call camera_init, in case of low power it should be possible to control the motors and let the camera switched off

  // Wifi setup
  WiFi.persistent(true);

  uint8_t macAddr[6];
  WiFi.macAddress(macAddr);
#if defined(USE_SOFTAP)
  WiFi.disconnect();
  /* set up access point */
  WiFi.mode(WIFI_AP);

  char ssidmac[33];
  sprintf(ssidmac, "%s%02X%02X", ssid, macAddr[4], macAddr[5]); // ssidmac = ssid + 4 hexadecimal values of MAC address
  WiFi.softAP(ssidmac, password, WIFI_SOFTAP_CHANNEL);
  IPAddress apIP = WiFi.softAPIP();
#ifdef DEBUG_SERIAL
  DEBUG_SERIAL.print(F("SoftAP SSID="));
  DEBUG_SERIAL.print(ssidmac);
  DEBUG_SERIAL.print(F("IP: "));
  DEBUG_SERIAL.println(apIP);
#endif
  /* Setup the DNS server redirecting all the domains to the apIP */
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(53, "z.be", apIP);
#else // USE_SOFTAP not defined
  WiFi.softAPdisconnect(true);
  // host_name = "BlimpCam-" + 6 hexadecimal values of MAC address
  char host_name[33];
  sprintf(host_name, "BlimpCam-%02X%02X%02X", macAddr[3], macAddr[4], macAddr[5]);
#ifdef DEBUG_SERIAL
  DEBUG_SERIAL.print(F("Hostname: "));
  DEBUG_SERIAL.println(host_name);
#endif
  WiFi.setHostname(host_name);

  // Connect to wifi
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(ssid, password);

  // Wait some time to connect to wifi
  for (int i = 0; i < 15 && WiFi.status() != WL_CONNECTED; i++) {
#ifdef DEBUG_SERIAL
    DEBUG_SERIAL.print('.');
#endif
    delay(1000);
  }

#ifdef DEBUG_SERIAL
  DEBUG_SERIAL.print(F("\nWiFi connected - IP address: "));
  DEBUG_SERIAL.println(WiFi.localIP());   // You can get IP address assigned to ESP
#endif

  if (!MDNS.begin(host_name)) {
#ifdef DEBUG_SERIAL
    DEBUG_SERIAL.println("Error starting mDNS");
#endif
    return;
  }


  MDNS.addService("http", "tcp", 80); // Add http service to MDNS-SD
  MDNS.addService("ws", "tcp", 82); // Add websocket service to MDNS-SD

#endif // USE_SOFTAP

  webserver.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
#ifdef DEBUG_SERIAL
    DEBUG_SERIAL.println(F("on HTTP_GET: return"));
#endif
    request->send(200, "text/html", index_html);
  });

  webserver.begin();
  server.listen(82);
#ifdef DEBUG_SERIAL
  DEBUG_SERIAL.print(F("Is server live? "));
  DEBUG_SERIAL.println(server.available());
#endif
  last_activity_message = millis();
}

void handleSlider(int value)
{
#ifdef DEBUG_SERIAL
  // DEBUG_SERIAL.print(F("handleSlider value="));
  // DEBUG_SERIAL.println(value);
#endif
  s1 = value;
}

void handleSwitch(int value)
{
// #ifdef DEBUG_SERIAL
//   // DEBUG_SERIAL.print(F("handleSwitch value="));
//   // DEBUG_SERIAL.println(value);
// #endif
#ifdef USE_CAMERA
  videoswitch = value;
  // DEBUG_SERIAL.print(F("videoSwitch value="));
  // DEBUG_SERIAL.println(value);
#endif
}

void handleButtons(int value1, int value2)
{
// #ifdef DEBUG_SERIAL
//   // DEBUG_SERIAL.print(F("handleButton1 value="));
//   // DEBUG_SERIAL.println(value1);
//   // DEBUG_SERIAL.print(F("handleButton2 value="));
//   // DEBUG_SERIAL.println(value2);
// #endif
  ccwbtn = value1;
  cwbtn = value2;
}

void handleJoystick(int x, int y)
{
// #ifdef DEBUG_SERIAL
//   // DEBUG_SERIAL.print(F("handleJoystick x="));
//   // DEBUG_SERIAL.print(x);
//   // DEBUG_SERIAL.print(F(" y="));
//   // DEBUG_SERIAL.println(y);
// #endif
  jx = x;
  jy = y;
  
}

void spitout(){
#ifdef DEBUG_SERIAL
  // DEBUG_SERIAL.println(msgstr);
  // DEBUG_SERIAL.print(F("DT:"));
  // DEBUG_SERIAL.print(int((s1/180*255)));
  // DEBUG_SERIAL.print(F(":"));
  // DEBUG_SERIAL.print(int(((jx+180)/360*255)));
  // DEBUG_SERIAL.print(F(":"));
  // DEBUG_SERIAL.print(int(((jy+180)/360*255)));
  // DEBUG_SERIAL.print(F(":"));
  // DEBUG_SERIAL.print((cwbtn*255));
  // DEBUG_SERIAL.print(F(":"));
  // DEBUG_SERIAL.print((ccwbtn*255));
  DEBUG_SERIAL.print(F("*"));
  DEBUG_SERIAL.print(int(s1));
  DEBUG_SERIAL.print(F(":"));
  DEBUG_SERIAL.print(int(jx));
  DEBUG_SERIAL.print(F(":"));
  DEBUG_SERIAL.print(int(jy));
  DEBUG_SERIAL.print(F(":"));
  DEBUG_SERIAL.print(int(cwbtn));
  DEBUG_SERIAL.print(F(":"));
  DEBUG_SERIAL.print(int(ccwbtn));
  DEBUG_SERIAL.println(F("#"));


#endif
  // ledcWrite(0,map(s1,0,180,0,4095));
  // ledcWrite(0,map(jx,-180,180,0,4095));
  // ledcWrite(0,map(jy,-180,180,0,4095));
  // ledcWrite(0,map(cwbtn,0,1,0,4095));
  // ledcWrite(0,map(ccwbtn,0,1,0,4095));
  
  // ledcWrite(0,map(s1,0,180,0,4095));
  // ledcWrite(0,map(s1,0,180,0,4095));
  // ledcWrite(1,int(((jx+180)/360*4095)));
  // ledcWrite(2,int(((jy+180)/360*4095)));
  // ledcWrite(3,int((cwbtn*4095)));
  // ledcWrite(4,int((ccwbtn*4095)));
}


void handle_message(websockets::WebsocketsMessage msg) {
  const char *msgstr = msg.c_str();
  const char *p;

#ifdef DEBUG_SERIAL
  // DEBUG_SERIAL.println();
  // DEBUG_SERIAL.print(F("handle_message \n"));
#endif
  spitout();
  // DEBUG_SERIAL.print(F(jx+':'+jy+':'+cwbtn+':'+ccwbtn+':'+s1));
  int id = atoi(msgstr);
  int param1 = 0;
  int param2 = 0;


  p = strchr(msgstr, ':');
  if (p)
  {
    param1 = atoi(++p);
    p = strchr(p, ',');
    if (p)
    {
      param2 = atoi(++p);
    }
  }



  led_set(LED_BRIGHTNESS_HANDLEMESSAGE);

  last_activity_message = millis();

  switch (id)
  {
    case 0:       // ping
      break;

    case 1:
      handleJoystick(param1, param2);
      break;

    case 2: handleSlider(param1);
      break;

    case 3: handleButtons(param1, param2);
      break;

    case 4: handleSwitch(param1);
      break;
  }
}


void onConnect()
{
  led_set(LED_BRIGHTNESS_CONNECTED);

#ifdef DEBUG_SERIAL
  DEBUG_SERIAL.println(F("onConnect"));
#endif
}

void onDisconnect()
{
#ifdef DEBUG_SERIAL
  DEBUG_SERIAL.println(F("onDisconnect"));
#endif
}

void loop()
{
  static unsigned long millis_next_camera = 0;
  static int is_connected = 0;

#if defined(USE_SOFTAP)
  dnsServer.processNextRequest();
#endif

  if (millis() > last_activity_message + TIMEOUT_MS_LED)
  {
    led_set(LED_BRIGHTNESS_OFF);
  }

  if (millis() > last_activity_message + TIMEOUT_MS_MOTORS)
  {
    // jy = 0;
    // jx = 0;
    // // s1 = 0;
    // cwbtn = 0;
    // ccwbtn = 0;
    spitout();
#ifdef DEBUG_SERIAL
    // DEBUG_SERIAL.println(F("Safety shutdown ..."));
#endif
    last_activity_message = millis();
  }

  if (is_connected)
  {
    if (sclient.available()) { // als return non-nul, dan is er een client geconnecteerd
      sclient.poll(); // als return non-nul, dan is er iets ontvangen

#ifdef USE_CAMERA
      if (videoswitch)
      {
        if (!camera_initialised)
        {
          camera_init(); // if already initialised, returns quickly
          millis_next_camera = millis() + 500; // wait some time after camera initialisation before taking first picture
        }
        if (millis() >= millis_next_camera )
        {
          fb = esp_camera_fb_get();
          if (fb)
          {
            sclient.sendBinary((const char *)fb->buf, fb->len);
            esp_camera_fb_return(fb);
            fb = NULL;
          }
          millis_next_camera = millis() + MIN_TIME_PER_FRAME;
        }
      }
#endif

    }
    else
    {
      // no longer connected
      onDisconnect();
      is_connected = 0;
    }
  }
  if (server.poll()) // if a new socket connection is requested
  {
#ifdef DEBUG_SERIAL
    DEBUG_SERIAL.print(F("server.poll is_connected="));
    DEBUG_SERIAL.println(is_connected);
#endif
    if (is_connected) {
      sclient.send("CLOSE");
    }
    sclient = server.accept();
#ifdef DEBUG_SERIAL
    DEBUG_SERIAL.println(F("Connection accept"));
#endif
    sclient.onMessage(handle_message);
    onConnect();
    is_connected = 1;
  }

  if (!is_connected)
  {
    led_set((millis() % 1000) > 500 ? LED_BRIGHTNESS_NO_CONNECTION : LED_BRIGHTNESS_OFF);
  }
  // spitout();
  
}
