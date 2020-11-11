#include <Arduino.h>
#include <WiFi.h>
#include "Arduino.h"
#include "stdio.h"


#include "soc/soc.h"          // Disable brownour problems
#include "soc/rtc_cntl_reg.h" // Disable brownour problems

//html
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <FS.h>

//mqtt
#include <PubSubClient.h>

//mqtt configuration
const char* mqtt_server = "192.168.0.50";
WiFiClient espClient;
PubSubClient client(espClient);

// wifi configuration
const char *ssid = "CLV";
const char *password = "Pi@Riya*1";


bool internet_connected = false;
struct tm timeinfo;
time_t now;
char strftime_buf[64]; //time for webserver
char c_relayBitH [8] = "1" ;
char c_relayBitL [8] = "0" ;
char C_ip_adress [14] = "IP adress" ;//for Mqtt ID
char C_mac_adr[18]; //for Mqtt ID
char C_idHostname[40];
int LevelSensorPIN = 15;  
int Ledboard = 2;
int RelayCtl = 13;

//delay multiple
#define uS_TO_S_FACTOR 1000000
#define mS_TO_S 1000

//my time
// int day, hours, minutes, seconds, year, month, date, minuteSave;
// WiFiUDP ntpUDP;
// NTPClient timeClient(ntpUDP, "0.pool.ntp.org", 25200, 0);
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 25200;
const int daylightOffset_sec = 0;

//variable for webserver
const char *PARAM_waitToActive = "waitToActive";
const char *PARAM_pulseToPump = "pulseToPump";
const char *PARAM_ipAdress = "ipAdress";
const char *PARAM_macAdress = "macAdress";
const char *PARAM_idHostname = "idHostname";


//var delay pump
int Int_waitToActive;
int Int_pulseToPump;
String S_waitToActive;
String S_pulseToPump;
String S_ipAdress;
String S_macAdress;
String S_idHostname;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

//SPIFFS read & write
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
  WiFi.config(INADDR_NONE,INADDR_NONE,INADDR_NONE,INADDR_NONE);

  S_idHostname = readFile(SPIFFS, "/idHostname.txt");
  S_idHostname.toCharArray(C_idHostname,40);
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
    Serial.printf("Waiting for system time to be set... (%d/%d)\n", retry, retry_count);
    delay(2000);
    time(&now);
    localtime_r(&now, &timeinfo);
  }
}



//Processor read back to value on website
String processor(const String &var)
{
  if (var == "waitToActive")
  {
    //Read waitToActive :
    S_waitToActive = readFile(SPIFFS, "/waitToActive.txt");
    Int_waitToActive = S_waitToActive.toInt();
    return readFile(SPIFFS, "/waitToActive.txt");
  }
  else if (var == "pulseToPump")
  {
    //Read pulseToPump
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
//SPIFFS read & write


void init_server() //Server init
{
  File file = SPIFFS.open("/index.html", "r");
  if (!file)
  {
    Serial.println("file open failed");
  }
  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });
 
    //Read waitToActive :
    S_waitToActive = readFile(SPIFFS, "/waitToActive.txt");
    Int_waitToActive = S_waitToActive.toInt();

    //Read pulseToPump
    S_pulseToPump = readFile(SPIFFS, "/pulseToPump.txt");
    Int_pulseToPump = S_pulseToPump.toInt();

    //Read hostname
    S_idHostname = readFile(SPIFFS, "/idHostname.txt");


  server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request) {
    ESP.restart();
  });

  // Send a GET request to <ESP_IP>/get?input1=<inputMessage>
  server.on("/get", HTTP_GET, [](AsyncWebServerRequest *request) {
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

    else
    {
      inputMessage = "No message sent";
    }
    request->send(200, "text/text", inputMessage);
  });
  server.begin();
} //end Server init

// call back mqtt
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  String messageTemp;
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  // Switch on the LED if an 1 was received as first character
  if (String(topic) == "esp32/output") {
    Serial.print("Changing output to ");
    if(messageTemp == "on"){
      Serial.println("on");
      digitalWrite(Ledboard, HIGH);
    }
    else if(messageTemp == "off"){
      Serial.println("off");
      digitalWrite(Ledboard, LOW);
    }
  }
}

void reconnect() //reconnect mqtt server
{ 
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "homeB001";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("outTopic", "hello world");
      // ... and resubscribe
      client.subscribe("esp32/output");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}


void setup() {
  // put your setup code here, to run once:
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
  Serial.begin(115200);
  
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
  
    WiFi.localIP().toString().toCharArray(C_ip_adress,14); // Convert IP adress to String then to Char Array
    WiFi.macAddress().toCharArray(C_mac_adr,18); // Convert Mac adr to Char array

    Serial.write("Wifi connected");
  }
   //check SPIFFS
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
 
  init_server(); //start server
  client.setServer(mqtt_server, 1883); //start mqtt
  client.setCallback(callback);



  //Init pin mode 
  pinMode(LevelSensorPIN,INPUT_PULLDOWN);
  pinMode(Ledboard,OUTPUT);
  pinMode(RelayCtl,OUTPUT);
}

void loop() {
 
  // put your main code here, to run repeatedly:
  client.loop();
  if (!client.connected())
   {
    reconnect();
  }
  if (digitalRead(LevelSensorPIN) == LOW) 
  {
    delay(50);
    if (digitalRead(LevelSensorPIN)== LOW) 
    {
      delay(Int_waitToActive*mS_TO_S);
      if (digitalRead(LevelSensorPIN)== LOW)
      {
        
        digitalWrite(RelayCtl,HIGH);
        digitalWrite(Ledboard,HIGH);
        client.publish("esp32/active",c_relayBitH);
        
        delay(Int_pulseToPump*mS_TO_S);
        client.publish("esp32/active",c_relayBitL);
          //ip_adress = WiFi.localIP();
        client.publish("esp32/idIP",C_ip_adress);
        client.publish("esp32/idMac",C_mac_adr);
        client.publish("esp32/idHostname",C_idHostname);
        digitalWrite(RelayCtl,LOW);
        digitalWrite(Ledboard,LOW);
      }
    }
  }
    Serial.print ("Init wait to active : " );
    Serial.println (Int_waitToActive);
    Serial.print ("Init pulse length : " );
    Serial.println (Int_pulseToPump );
    digitalWrite(Ledboard,LOW);
    delay(500);
    digitalWrite(Ledboard,HIGH);
    delay(500);
}