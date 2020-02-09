#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <CuteBuzzerSounds.h>
/* User Includes Here*/
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
//#include <SPI.h>

 /* Variables */
const char* host = "esp32";
const char* ssid = "BSSID";
const char* password = "wifi_pwd";

WebServer server(80);

/* User Variables Here */
// set to 1 if we are implementing the user interface pot, switch, etc
#define USE_UI_CONTROL 0

#if USE_UI_CONTROL
#include <MD_UISwitch.h>
#endif

// Turn on debug statements to the serial output
#define DEBUG 0

#if DEBUG
#define PRINT(s, x) { Serial.print(F(s)); Serial.print(x); }
#define PRINTS(x) Serial.print(F(x))
#define PRINTX(x) Serial.println(x, HEX)
#else
#define PRINT(s, x)
#define PRINTS(x)
#define PRINTX(x)
#endif

// Define the number of devices we have in the chain and the hardware interface
// NOTE: These pin numbers will probably not work with your hardware and may
// need to be adapted
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4
#define CLK_PIN   14
#define DATA_PIN  13
#define CS_PIN    15

#define BUZZER   12
#define LED_RED  32
#define LED_GREEN 33
#define RF433 35

int freq = 500;
int channel = 0;
int resolution = 8;

/* RTOS TASKS */
void TaskBlinkLed( void *pvParameters );
void TaskBuzzerPlay( void *pvParameters );
void TaskUpdateTime( void * pvParameters );

MD_Parola P = MD_Parola(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "0.au.pool.ntp.org", 3600, 60000);

uint8_t scrollSpeed = 35;    // default frame delay value
textEffect_t scrollEffect = PA_SCROLL_LEFT;
textPosition_t scrollAlign = PA_LEFT;
uint16_t scrollPause = 3000; // in milliseconds

// Global message buffers shared by Serial and Scrolling functions
#define  BUF_SIZE  75
char curMessage[BUF_SIZE] = { "Hello" };
char newMessage[BUF_SIZE] = { "Enter Message" };
bool newMessageAvailable = true;

void readSerial(void)
{
  static char *cp = newMessage;

  while (Serial.available())
  {

    *cp = (char)Serial.read();
    if ((*cp == '\n') || (cp - newMessage >= BUF_SIZE-2)) // end of message character or full buffer
    {
      *cp = '\0'; // end the string
      // restart the index for next filling spree and flag we have a message waiting
      cp = newMessage;
      newMessageAvailable = true;
    }
    else  // move char pointer to next position
      cp++;
  }
}

void TaskBlinkLed(void *pvParameters)
{
  (void) pvParameters;
 
    for (;;) // A Task shall never return or exit.
  {
    // RED
    digitalWrite(LED_RED, HIGH);   // turn the LED on (HIGH is the voltage level)
    vTaskDelay(1000);  // one tick delay (15ms) in between reads for stability
    // ORANGE
    digitalWrite (LED_GREEN, HIGH);  // turn on the LED
    vTaskDelay(1000); // wait for half a second or 500 milliseconds
    digitalWrite (LED_RED, LOW);  // turn off the LED
    // GREEN
    vTaskDelay(1000);  // one tick delay (15ms) in between reads for stability
    digitalWrite (LED_GREEN, LOW); 
  }
}

void TaskBuzzerPlay(void *pvParameters)
{

  int sounds[21] = {
   S_CONNECTION   ,S_DISCONNECTION ,S_BUTTON_PUSHED   
  ,S_MODE1        ,S_MODE2         ,S_MODE3     
  ,S_SURPRISE     ,S_OHOOH         ,S_OHOOH2    
  ,S_CUDDLY       ,S_SLEEPING      ,S_HAPPY     
  ,S_SUPER_HAPPY  ,S_HAPPY_SHORT   ,S_SAD       
  ,S_CONFUSED     ,S_FART1         ,S_FART2     
  ,S_FART3        ,PIRATES         ,S_JUMP};

  int index;

  for(;;){
    index = random(0, 21);
    //cute.play(sounds[index]);
    vTaskDelay(60000);
  }
 
  //  (void) pvParameters;
  //  ledcAttachPin(BUZZER, channel);
  //  vTaskDelay(300);
  //  ledcWrite(channel, 50);
  //  vTaskDelay(200);
  //  ledcWrite(channel, 25);
  //  vTaskDelay(100);
  //  ledcDetachPin(BUZZER);
  //  vTaskDelete( NULL );
}

void TaskDisplayText(void *pvParameters)
{
  for(;;){
    if (P.displayAnimate())
    {
      if ((bool *)pvParameters)
      {
        vTaskDelay(10);
      }
      P.displayReset();
    }
  }
}

void TaskUpdateTime(void *pvparameter)
{
  for(;;){
    while (!timeClient.update())
    {
      timeClient.forceUpdate();
    }
    vTaskDelay(1000);
  }
}

/*
 * Login page
 */

const char* loginIndex = 
 "<form name='loginForm'>"
    "<table width='20%' bgcolor='A09F9F' align='center'>"
        "<tr>"
            "<td colspan=2>"
                "<center><font size=4><b>ESP32 Login Page</b></font></center>"
                "<br>"
            "</td>"
            "<br>"
            "<br>"
        "</tr>"
        "<td>Username:</td>"
        "<td><input type='text' size=25 name='userid'><br></td>"
        "</tr>"
        "<br>"
        "<br>"
        "<tr>"
            "<td>Password:</td>"
            "<td><input type='Password' size=25 name='pwd'><br></td>"
            "<br>"
            "<br>"
        "</tr>"
        "<tr>"
            "<td><input type='submit' onclick='check(this.form)' value='Login'></td>"
        "</tr>"
    "</table>"
"</form>"
"<script>"
    "function check(form)"
    "{"
    "if(form.userid.value=='admin' && form.pwd.value=='admin')"
    "{"
    "window.open('/serverIndex')"
    "}"
    "else"
    "{"
    " alert('Error Password or Username')/*displays error message*/"
    "}"
    "}"
"</script>";
 
/*
 * Server Index Page
 */
 
const char* serverIndex = 
"<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
"<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
   "<input type='file' name='update'>"
        "<input type='submit' value='Update'>"
    "</form>"
 "<div id='prg'>progress: 0%</div>"
 "<script>"
  "$('form').submit(function(e){"
  "e.preventDefault();"
  "var form = $('#upload_form')[0];"
  "var data = new FormData(form);"
  " $.ajax({"
  "url: '/update',"
  "type: 'POST',"
  "data: data,"
  "contentType: false,"
  "processData:false,"
  "xhr: function() {"
  "var xhr = new window.XMLHttpRequest();"
  "xhr.upload.addEventListener('progress', function(evt) {"
  "if (evt.lengthComputable) {"
  "var per = evt.loaded / evt.total;"
  "$('#prg').html('progress: ' + Math.round(per*100) + '%');"
  "}"
  "}, false);"
  "return xhr;"
  "},"
  "success:function(d, s) {"
  "console.log('success!')" 
 "},"
 "error: function (a, b, c) {"
 "}"
 "});"
 "});"
 "</script>";

/*
 * setup function
 */
void setup(void) {
  Serial.begin(115200);

  /* User Code Here*/
  P.begin();
  P.displayText(curMessage, scrollAlign, scrollSpeed, scrollPause, scrollEffect, scrollEffect);

  //ledcSetup(channel, freq, resolution);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);

  /* SETUP TASKS */
  xTaskCreatePinnedToCore(
      TaskBlinkLed, "TaskBlinkLed" // A name just for humans
      ,
      1024 // This stack size can be checked & adjusted by reading the Stack Highwater
      ,
      NULL, 1 // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
      ,
      NULL, 0);

  xTaskCreatePinnedToCore(
      TaskBuzzerPlay, "TaskBuzzerPlay" // A name just for humans
      ,
      1024 // This stack size can be checked & adjusted by reading the Stack Highwater
      ,
      NULL, 2 // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
      ,
      NULL, 0);

  xTaskCreatePinnedToCore(
      TaskDisplayText, "TaskDisplayText" // A name just for humans
      ,
      1024 // This stack size can be checked & adjusted by reading the Stack Highwater
      ,
      (void *)newMessageAvailable, 1 // Priority, with 3 (configMAX_PRIORITIES - 1) b  eing the highest, and 0 being the lowest.
      ,
      NULL, 1);

    cute.init(BUZZER);
    
  // Connect to WiFi network
  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  /*use mdns for host name resolution*/
  if (!MDNS.begin(host)) { //http://esp32.local
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }
  Serial.println("mDNS responder started");
  /*return index page which is stored in serverIndex */
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", loginIndex);
  });
  server.on("/serverIndex", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverIndex);
  });
  /*handling uploading firmware file */
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });
  server.begin();

    xTaskCreatePinnedToCore(
    TaskUpdateTime
    ,"TaskUpdateTime"
    ,1024
    ,NULL
    ,1
    ,NULL
    ,0);

    timeClient.begin();
    timeClient.setTimeOffset(36000); // GMT +10
}

void loop(void) {
  server.handleClient();
  delay(1);
  /* User Code Here*/
  if (newMessageAvailable)
    {
      strcpy(curMessage, newMessage);
      newMessageAvailable = false;
      }
    
  readSerial();
}
