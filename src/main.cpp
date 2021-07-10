#include <Arduino.h>
#include <WiFi.h>
#include "Arduino.h"
#include "stdio.h"

#include "soc/soc.h"          // Disable brownour problems
#include "soc/rtc_cntl_reg.h" // Disable brownour problems

// html
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <FS.h>

// mqtt
#include <PubSubClient.h>

// OTA
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

// timer interrupt
volatile int interruptCounter;
int timerCount;          // test statement for each step in second
int timerCountInactive;  // test statement for inactive each step in second
boolean flagEx = false;  // flag to excute 1 time the statement
boolean flagLvl = false; // flag from water level sensor
boolean flagInactive = false;
unsigned int timer1s;
boolean cycling = false;
unsigned int intResetCount;
char charResetCount[200];
String S_ResetCount;

hw_timer_t *timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

// mqtt configuration
const char *mqtt_server = "192.168.0.50";
WiFiClient espClient;
PubSubClient client(espClient);

// wifi configuration
const char *ssid = "CLV";
const char *password = "Pi@Riya*1";

bool internet_connected = false;
struct tm timeinfo;
time_t now;
char strftime_buf[64]; // time for webserver
char c_relayBitH[8] = "1";
char c_relayBitL[8] = "0";
char C_ip_adress[14] = "IP adress"; // for Mqtt ID
char C_mac_adr[18];                 // for Mqtt ID
char C_idHostname[40];
char C_topic_Hostname[40] = "esp32/";
char C_timeCount[8];
int LevelSensorPIN = 15;
int Ledboard = 2;
int RelayCtlIn = 14;
int RelayCtlOut = 13;
int mQtyFailCt = 5;

int i = 5;  // variable for loop
int y = 10; // variable for wifi reset
// delay multiple
#define uS_TO_S_FACTOR 1000000
#define mS_TO_S 1000

// my time
// int day, hours, minutes, seconds, year, month, date, minuteSave;
// WiFiUDP ntpUDP;
// NTPClient timeClient(ntpUDP, "0.pool.ntp.org", 25200, 0);
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 25200;
const int daylightOffset_sec = 0;

// variable for webserver
const char *PARAM_waitToActive = "waitToActive";
const char *PARAM_pulseToPump = "pulseToPump";
const char *PARAM_ipAdress = "ipAdress";
const char *PARAM_macAdress = "macAdress";
const char *PARAM_idHostname = "idHostname";
const char *PARAM_waitToStop = "waitToStop";
const char *PARAM_pulseToStop = "pulseToStop";
const char *PARAM_resetCount = "resetCount";
const char *PARAM_timeCount = "timeCount";
const char *PARAM_2ndPSt = "2ndPSt";
const char *PARAM_2ndPSpulseLenght = "2ndPSpulseLenght";
const char *PARAM_inactiveP = "inactiveP";
const char *PARAM_inactivePulseLenght = "inactivePulseLenght";

// var delay pump
int Int_waitToActive;
int Int_pulseToPump;
int Int_waitToStop;
int Int_pulseToStop;
int Int_2ndPSt;              // delay to active again pump
int Int_2ndPSpulseLenght;    // pulse lengnt for second pump
int Int_inactiveP;           // inactive pump
int Int_inactivePulseLenght; //pulse lenght inactive pump

int Int_WtaPtp = 0; // sum Int_waitToActive + Int_pulseToPump
int Int_WtaPtpWts =
    0;                    // sum Int_waitToActive + Int_pulseToPump + Int_waitToStop
int Int_WtaPtpWtsPts = 0; // sum Int_waitToActive + Int_pulseToPump +
                          // Int_waitToStop + Int_pulseToStop
// int Int_delaySiphon;

String S_waitToActive;
String S_pulseToPump;
String S_ipAdress;
String S_macAdress;
String S_idHostname;
String S_waitToStop;
String S_pulseToStop;
String S_2ndPSt;              // delay to active again pump
String S_2ndPSpulseLenght;    // pulse lengnt for second pump
String S_inactiveP;           // inactive pump
String S_inactivePulseLenght; // inactive pulse lenght

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// SPIFFS read & write
String readFile(fs::FS &fs, const char *path)
{
  Serial.printf("Reading file: %s\r\n", path);
  File file = fs.open(path, "r");
  if (!file || file.isDirectory())
  {
    Serial.println("- empty file or failed to open file");
    return String();
  }
  Serial.println("- read from file:");
  String fileContent;
  while (file.available())
  {
    fileContent += String((char)file.read());
  }
  Serial.println(fileContent);
  return fileContent;
}

bool init_wifi()
{
  int connAttempts = 0;
  Serial.println("\r\nConnecting to: " + String(ssid));
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);

  S_idHostname = readFile(SPIFFS, "/idHostname.txt");
  S_idHostname.toCharArray(C_idHostname, 40);
  WiFi.setHostname(C_idHostname);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(2000);
    Serial.print(".");
    if (connAttempts > 10)
      return false;
    connAttempts++;
  }
  return true;
}

void init_time()
{
  struct tm timeinfo;
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  // wait for time to be set
  time_t now = 0;
  timeinfo = {0};
  int retry = 0;
  const int retry_count = 10;
  while (timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count)
  {
    Serial.printf("Waiting for system time to be set... (%d/%d)\n", retry,
                  retry_count);
    delay(2000);
    time(&now);
    localtime_r(&now, &timeinfo);
  }
}

// Processor read back to value on website
String processor(const String &var)
{
  if (var == "waitToActive")
  {
    // Read waitToActive :
    S_waitToActive = readFile(SPIFFS, "/waitToActive.txt");
    Int_waitToActive = S_waitToActive.toInt();
    return readFile(SPIFFS, "/waitToActive.txt");
  }
  else if (var == "pulseToPump")
  {
    // Read pulseToPump
    S_pulseToPump = readFile(SPIFFS, "/pulseToPump.txt");
    Int_pulseToPump = S_pulseToPump.toInt();
    return readFile(SPIFFS, "/pulseToPump.txt");
  }
  else if (var == "idHostname")
  {
    S_idHostname = readFile(SPIFFS, "/idHostname.txt");
    return readFile(SPIFFS, "/idHostname.txt");
  }
  else if (var == "timeNow")
  {
    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%F_%H_%M_%S", &timeinfo);
    return String(strftime_buf);
  }
  else if (var == "ipAdress")
  {
    return String(WiFi.localIP().toString());
  }
  else if (var == "macAdress")
  {
    return String(WiFi.macAddress());
  }
  else if (var == "timeCount")
  {
    return String(timerCount);
  }
  else if (var == "timerCountInactive")
  {
    return String(timerCountInactive);
  }
  else if (var == "resetCount")
  {
    return readFile(SPIFFS, "/resetCount.txt");
  }
  else if (var == "waitToStop")
  {
    // Read waitToStop :
    S_waitToStop = readFile(SPIFFS, "/waitToStop.txt");
    Int_waitToStop = S_waitToStop.toInt();
    return readFile(SPIFFS, "/waitToStop.txt");
  }
  else if (var == "pulseToStop")
  {
    // Read waitToStop :
    S_pulseToStop = readFile(SPIFFS, "/pulseToStop.txt");
    Int_pulseToStop = S_pulseToStop.toInt();
    return readFile(SPIFFS, "/pulseToStop.txt");
  }
  else if (var == "2ndPSt")
  {
    S_2ndPSt = readFile(SPIFFS, "/2ndPSt.txt");
    Int_2ndPSt = S_2ndPSt.toInt();
    return readFile(SPIFFS, "/2ndPSt.txt");
  }
  else if (var == "2ndPSpulseLenght")
  {
    S_2ndPSpulseLenght = readFile(SPIFFS, "/2ndPSpulseLenght.txt");
    Int_2ndPSpulseLenght = S_2ndPSpulseLenght.toInt();
    return readFile(SPIFFS, "/2ndPSpulseLenght.txt");
  }
  else if (var == "inactiveP")
  {
    S_inactiveP = readFile(SPIFFS, "/inactiveP.txt");
    Int_inactiveP = S_inactiveP.toInt();
    return readFile(SPIFFS, "/inactiveP.txt");
  }
  else if (var == "inactivePulseLenght")
  {
    S_inactivePulseLenght = readFile(SPIFFS, "/inactivePulseLenght.txt");
    Int_inactivePulseLenght = S_inactivePulseLenght.toInt();
    return readFile(SPIFFS, "/inactivePulseLenght.txt");
  }

  return String();
}

void writeFile(fs::FS &fs, const char *path, const char *message)
{
  Serial.printf("Writing file: %s\r\n", path);
  File file = fs.open(path, "w");
  if (!file)
  {
    Serial.println("- failed to open file for writing");
    return;
  }
  if (file.print(message))
  {
    Serial.println("- file written");
  }
  else
  {
    Serial.println("- write failed");
  }
}
// SPIFFS read & write

void init_server() // Server init
{
  File file = SPIFFS.open("/index.html", "r");
  if (!file)
  {
    Serial.println("file open failed");
  }
  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/index.html", String(), false, processor); });

  // Read waitToActive :
  S_waitToActive = readFile(SPIFFS, "/waitToActive.txt");
  Int_waitToActive = S_waitToActive.toInt();

  // Read pulseToPump
  S_pulseToPump = readFile(SPIFFS, "/pulseToPump.txt");
  Int_pulseToPump = S_pulseToPump.toInt();

  // Read pulseToStop
  S_pulseToStop = readFile(SPIFFS, "/pulseToStop.txt");
  Int_pulseToStop = S_pulseToStop.toInt();

  // Read waitToStop :
  S_waitToStop = readFile(SPIFFS, "/waitToStop.txt");
  Int_waitToStop = S_waitToStop.toInt();

  // Read second pump every second
  S_2ndPSt = readFile(SPIFFS, "/2ndPSt.txt");
  Int_2ndPSt = S_2ndPSt.toInt();

  // Read lenght second pump every second
  S_2ndPSpulseLenght = readFile(SPIFFS, "/2ndPSpulseLenght.txt");
  Int_2ndPSpulseLenght = S_2ndPSpulseLenght.toInt();

  // Read inactive pump
  S_inactiveP = readFile(SPIFFS, "/inactiveP.txt");
  Int_inactiveP = S_inactiveP.toInt();

  // Read inactive pump lenght
  S_inactivePulseLenght = readFile(SPIFFS, "/inactivePulseLenght.txt");
  Int_inactivePulseLenght = S_inactivePulseLenght.toInt();

  // Read hostname
  S_idHostname = readFile(SPIFFS, "/idHostname.txt");

  // //Read Reset Count
  // S_ResetCount = readFile(SPIFFS, "/resetCount.txt");

  server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              writeFile(SPIFFS, "/resetCount.txt", "0");
              delay(10);
              ESP.restart();
            });

  server.on("/AirOutOn", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              digitalWrite(RelayCtlOut, HIGH);
              Serial.println("Relay Air Out on");
              delay(1000);
              request->redirect("/");
            });
  server.on("/AirOutOff", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              digitalWrite(RelayCtlOut, LOW);
              Serial.println("Relay Air Out off");
              delay(1000);
              request->redirect("/");
            });
  server.on("/AirInOn", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              digitalWrite(RelayCtlIn, HIGH);
              Serial.println("Relay Air In on");
              delay(1000);
              request->redirect("/");
            });
  server.on("/AirInOff", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              digitalWrite(RelayCtlIn, LOW);
              Serial.println("Relay Air In off");
              delay(1000);
              request->redirect("/");
            });

  // Send a GET request to <ESP_IP>/get?input1=<inputMessage>
  server.on("/get", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              String inputMessage;
              // String inputParam; //no used
              // GET timeBetween value on <ESP_IP>/get?timeBetween=<inputMessage>
              if (request->hasParam(PARAM_waitToActive))
              {
                inputMessage = request->getParam(PARAM_waitToActive)->value();
                writeFile(SPIFFS, "/waitToActive.txt", inputMessage.c_str());
              }
              else if (request->hasParam(PARAM_pulseToPump))
              {
                inputMessage = request->getParam(PARAM_pulseToPump)->value();
                writeFile(SPIFFS, "/pulseToPump.txt", inputMessage.c_str());
              }
              else if (request->hasParam(PARAM_idHostname))
              {
                inputMessage = request->getParam(PARAM_idHostname)->value();
                writeFile(SPIFFS, "/idHostname.txt", inputMessage.c_str());
              }
              else if (request->hasParam(PARAM_waitToStop))
              {
                inputMessage = request->getParam(PARAM_waitToStop)->value();
                writeFile(SPIFFS, "/waitToStop.txt", inputMessage.c_str());
              }
              else if (request->hasParam(PARAM_pulseToStop))
              {
                inputMessage = request->getParam(PARAM_pulseToStop)->value();
                writeFile(SPIFFS, "/pulseToStop.txt", inputMessage.c_str());
              }
              else if (request->hasParam(PARAM_2ndPSt))
              {
                inputMessage = request->getParam(PARAM_2ndPSt)->value();
                writeFile(SPIFFS, "/2ndPSt.txt", inputMessage.c_str());
              }
              else if (request->hasParam(PARAM_2ndPSpulseLenght))
              {
                inputMessage = request->getParam(PARAM_2ndPSpulseLenght)->value();
                writeFile(SPIFFS, "/2ndPSpulseLenght.txt", inputMessage.c_str());
              }
              else if (request->hasParam(PARAM_inactiveP))
              {
                inputMessage = request->getParam(PARAM_inactiveP)->value();
                writeFile(SPIFFS, "/inactiveP.txt", inputMessage.c_str());
              }
              else if (request->hasParam(PARAM_inactivePulseLenght))
              {
                inputMessage = request->getParam(PARAM_inactivePulseLenght)->value();
                writeFile(SPIFFS, "/inactivePulseLenght.txt", inputMessage.c_str());
              }
              else
              {
                inputMessage = "No message sent";
              }
              request->send(200, "text/text", inputMessage);
            });
  server.begin();
} // end Server init

// call back mqtt
void callback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  String messageTemp;
  for (int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  // Switch on the LED if an 1 was received as first character
  if (String(topic) == "esp32/output")
  {
    Serial.print("Changing output to ");
    if (messageTemp == "on")
    {
      Serial.println("on");
      digitalWrite(Ledboard, HIGH);
    }
    else if (messageTemp == "off")
    {
      Serial.println("off");
      digitalWrite(Ledboard, LOW);
    }
  }
}

void reconnect() // reconnect mqtt server
{
  // Loop until we're reconnected
  if (!client.connected() && (mQtyFailCt >= 0))
  {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = C_idHostname;
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str()))
    {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("outTopic", "hello world");
      // ... and resubscribe
      client.subscribe("esp32/output");
      mQtyFailCt = 5;
    }
    else if (mQtyFailCt == 0)
    {
      Serial.println("Mqtt fail 5 time restart esp32");
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
      mQtyFailCt--;
    }
  }
}
// code OTA
void init_OTA()
{
  ArduinoOTA
      .onStart([]()
               {
                 String type;
                 if (ArduinoOTA.getCommand() == U_FLASH)
                   type = "sketch";
                 else // U_SPIFFS
                   type = "filesystem";

                 // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS
                 // using SPIFFS.end()
                 Serial.println("Start updating " + type);
               })
      .onEnd([]()
             { Serial.println("\nEnd"); })
      .onProgress([](unsigned int progress, unsigned int total)
                  { Serial.printf("Progress: %u%%\r", (progress / (total / 100))); })
      .onError([](ota_error_t error)
               {
                 Serial.printf("Error[%u]: ", error);
                 if (error == OTA_AUTH_ERROR)
                   Serial.println("Auth Failed");
                 else if (error == OTA_BEGIN_ERROR)
                   Serial.println("Begin Failed");
                 else if (error == OTA_CONNECT_ERROR)
                   Serial.println("Connect Failed");
                 else if (error == OTA_RECEIVE_ERROR)
                   Serial.println("Receive Failed");
                 else if (error == OTA_END_ERROR)
                   Serial.println("End Failed");
               });

  ArduinoOTA.begin();
}

void checkConnection()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    delay(10);
    Serial.println("Wifi Connected");
    y = 10;
  }
  else if ((WiFi.status() != WL_CONNECTED) && (y > 0) && (cycling == false))
  {
    WiFi.reconnect();
    delay(100);
    Serial.print("Wifi no connected : ");
    Serial.println(y);
    --y; // decrease in interrupt
  }
  else if (y == 0)
  {
    Serial.println("Wifi No Connected need to reboot");
    S_ResetCount = readFile(SPIFFS, "/resetCount.txt");
    intResetCount = S_ResetCount.toInt() + 1;
    writeFile(SPIFFS, "/resetCount.txt",
              itoa(intResetCount, charResetCount, 10));
    ESP.restart();
  }
}

void IRAM_ATTR onTimer()
{
  portENTER_CRITICAL_ISR(&timerMux);
  interruptCounter++;
  portEXIT_CRITICAL_ISR(&timerMux);
}

void setup()
{
  // put your setup code here, to run once:
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // disable brownout detector
  Serial.begin(115200);
  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 1000000, true);
  timerAlarmEnable(timer);

  if (!SPIFFS.begin(true))
  {
    Serial.println("An Error has occurred while mounting SPIFFS");
    ESP.restart();
  }
  else
  {
    delay(500);
    Serial.println("SPIFFS mounted successfully");
  }
  Serial.write("Hello world");
  if (init_wifi())
  { // Connected to WiFi
    internet_connected = true;
    Serial.println("Internet connected");
    // Print ESP32 Local IP Address
    Serial.println(WiFi.localIP());
    Serial.println(WiFi.macAddress());
    init_time();
    time(&now);
    // setenv("TZ", "GMT0BST,M3.5.0/01,M10.5.0/02", 1);
    // tzset();

    WiFi.localIP().toString().toCharArray(
        C_ip_adress, 14); // Convert IP adress to String then to Char Array
    WiFi.macAddress().toCharArray(C_mac_adr,
                                  18); // Convert Mac adr to Char array

    Serial.write("Wifi connected");
  }
  // check SPIFFS
  if (!SPIFFS.begin(true))
  {
    Serial.println("An Error has occurred while mounting SPIFFS");
    ESP.restart();
  }
  else
  {
    delay(500);
    Serial.println("SPIFFS mounted successfully");
  }

  init_server();                       // start server
  client.setServer(mqtt_server, 1883); // start mqtt
  client.setCallback(callback);
  strcat(C_topic_Hostname, C_idHostname); // topic preparation

  // Init pin mode
  pinMode(LevelSensorPIN, INPUT_PULLDOWN);
  pinMode(Ledboard, OUTPUT);
  pinMode(RelayCtlIn, OUTPUT);
  pinMode(RelayCtlOut, OUTPUT);
  digitalWrite(RelayCtlIn, LOW);
  digitalWrite(RelayCtlOut, LOW);

  // OTA init
  init_OTA();
}

void loop()
{
  if (timer1s > 0)
  {
    timer1s = 0;
    checkConnection();
  }
  if (!client.connected())
  {
    reconnect();
  }
  client.loop();
  ArduinoOTA.handle();

  if (digitalRead(LevelSensorPIN) == LOW)
  {
    delay(100);
    if (digitalRead(LevelSensorPIN) == LOW)
    {
      flagLvl = true;
      cycling = true;
      flagInactive = false;
      timerCountInactive = 0;
    }
  }

  Int_WtaPtp = Int_waitToActive + Int_pulseToPump;
  Int_WtaPtpWts = Int_waitToActive + Int_pulseToPump + Int_waitToStop;
  Int_WtaPtpWtsPts = Int_waitToActive + Int_pulseToPump + Int_waitToStop + Int_pulseToStop;

  // timer for excution different step.
  if (interruptCounter > 0)
  {
    portENTER_CRITICAL(&timerMux);
    interruptCounter = 0;
    portEXIT_CRITICAL(&timerMux);
    if (flagLvl) // active by lvl sensor
    {
      timerCount++;
      //writeFile(SPIFFS, "/timeCount.txt", itoa(timerCount, C_timeCount, 10));
      flagEx = false;
      Serial.print("timerCount_b: ");
      Serial.println(timerCount);
    }
    if (flagInactive)
    {
      timerCountInactive++;
      flagEx = false;
      Serial.print("timerCountInactive: ");
      Serial.println(timerCountInactive);
    }
    timer1s++;
  }

  if (timerCount == Int_waitToActive)
  {
    if (flagEx == false) // for executing 1 time
    {

      digitalWrite(Ledboard, HIGH);
      time(&now);
      localtime_r(&now, &timeinfo);
      strftime(strftime_buf, sizeof(strftime_buf), "%F_%H_%M_%S", &timeinfo);
      Serial.print("Time : ");
      Serial.println(strftime_buf);
      client.publish(C_topic_Hostname, c_relayBitH);
      Serial.print("High_topic :");
      Serial.println(C_topic_Hostname);
      digitalWrite(RelayCtlOut, HIGH);
      digitalWrite(RelayCtlIn, LOW);
      digitalWrite(Ledboard, HIGH);
      flagEx = true;
    }
  }
  else if (timerCount == Int_WtaPtp)
  {
    if (flagEx == false)
    {
      Serial.println("Stop Relay");
      digitalWrite(RelayCtlOut, LOW);
      digitalWrite(RelayCtlIn, LOW);
      digitalWrite(Ledboard, LOW);
      flagEx = true;
    }
  }
  else if ((timerCount == (Int_WtaPtp + Int_2ndPSt)) &&
           (timerCount < Int_WtaPtpWts))
  {
    if (flagEx == false)
    {
      Serial.println("Start Relay second time");
      digitalWrite(RelayCtlOut, HIGH);
      digitalWrite(Ledboard, HIGH);
      flagEx = true;
    }
  }
  else if ((timerCount == (Int_WtaPtp + Int_2ndPSt + Int_2ndPSpulseLenght)) &&
           (timerCount < Int_WtaPtpWts))
  {
    if (flagEx == false)
    {
      Serial.println("Stop Relay second time");
      digitalWrite(RelayCtlOut, LOW);
      digitalWrite(Ledboard, LOW);

      if (digitalRead(LevelSensorPIN) == LOW)
      {
        Serial.println("Still have water");
        timerCount = (timerCount -Int_2ndPSt - 1);
        //put value back for start second vacuum and -1 sec
      }
      else
      {
        Int_2ndPSt = Int_2ndPSt + S_2ndPSt.toInt();
      }
      
      //recheck if water is still present

      flagEx = true;
    }
  }

  else if (timerCount == Int_WtaPtpWts)
  {
    if (flagEx == false)
    {
      Serial.println("Air In");
      digitalWrite(RelayCtlIn, HIGH);
      digitalWrite(RelayCtlOut, LOW);
      flagEx = true;
    }
  }
  else if (timerCount == Int_WtaPtpWtsPts)
  {
    if (flagEx == false)
    {
      digitalWrite(RelayCtlIn, LOW);
      digitalWrite(RelayCtlOut, LOW);
      time(&now);
      localtime_r(&now, &timeinfo);
      strftime(strftime_buf, sizeof(strftime_buf), "%F_%H_%M_%S", &timeinfo);
      Serial.print("Time : ");
      Serial.println(strftime_buf);
      Serial.print("Low_topic :");
      Serial.println(C_topic_Hostname);
      client.publish(C_topic_Hostname, c_relayBitL);
      Serial.println("End cycle");
      digitalWrite(Ledboard, LOW);
      Int_2ndPSt = S_2ndPSt.toInt();
      timerCount = 0;
      flagEx = true;
      flagLvl = false; // flag for reading lvl sensor
      cycling = false;
      flagInactive = true; // flag for inactive
    }
  }

  else if (timerCount == 0)
  {
    digitalWrite(Ledboard, LOW);
    delay(500);
    digitalWrite(Ledboard, HIGH);
    delay(500);
    flagInactive = true;
  }

  if (timerCountInactive == Int_inactiveP)
  {
    if (flagEx == false)
    {
      Serial.println("Inactive On");
      digitalWrite(RelayCtlIn, HIGH);
      digitalWrite(RelayCtlOut, LOW);
      flagEx = true;
    }
  }
  if (timerCountInactive == Int_inactiveP + Int_inactivePulseLenght)
  {
    if (flagEx == false)
    {
      Serial.println("Inactive OFF");
      digitalWrite(RelayCtlIn, LOW);
      digitalWrite(RelayCtlOut, LOW);
      timerCountInactive = 0;
      flagEx = true;
    }
  }
}
