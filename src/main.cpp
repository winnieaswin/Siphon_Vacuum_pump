#include <Arduino.h>
#include <WiFi.h>
#include "Arduino.h"

#include "soc/soc.h"          // Disable brownour problems
#include "soc/rtc_cntl_reg.h" // Disable brownour problems

// wifi configuration

const char *ssid = "CLV";
const char *password = "Pi@Riya*1";

bool internet_connected = false;
struct tm timeinfo;
time_t now;


//Input pin
#define LevelSensorPIN 4 
#define Ledboard 2
#define RelayCtl 13

//delay multiple
#define uS_TO_S_FACTOR 1000000

//my time
// int day, hours, minutes, seconds, year, month, date, minuteSave;
// WiFiUDP ntpUDP;
// NTPClient timeClient(ntpUDP, "0.pool.ntp.org", 25200, 0);
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 25200;
const int daylightOffset_sec = 0;

bool init_wifi()
{
  int connAttempts = 0;
  Serial.println("\r\nConnecting to: " + String(ssid));
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

void setup() {
  // put your setup code here, to run once:
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
  Serial.begin(115200);
  Serial.write("Hello world");

    if (init_wifi())
  { // Connected to WiFi
    internet_connected = true;
    Serial.println("Internet connected");
    // Print ESP32 Local IP Address
    Serial.println(WiFi.localIP());
    init_time();
    time(&now);
    // setenv("TZ", "GMT0BST,M3.5.0/01,M10.5.0/02", 1);
    // tzset();
    Serial.write("Wifi connected");
  }

  //Init pin mode 
  pinMode(LevelSensorPIN,INPUT);
  pinMode(Ledboard, OUTPUT);
  pinMode(RelayCtl,OUTPUT);
}

void loop() {
  // put your main code here, to run repeatedly:
  if (LevelSensorPIN == LOW) 
  {
    digitalWrite(Ledboard,HIGH);
    digitalWrite(RelayCtl,HIGH);
    delay(2*uS_TO_S_FACTOR);
    
  }

    digitalWrite(RelayCtl,LOW);
    digitalWrite(Ledboard,LOW);
    delay(1*uS_TO_S_FACTOR);
    digitalWrite(Ledboard,HIGH);
    delay(1*uS_TO_S_FACTOR);

  

}