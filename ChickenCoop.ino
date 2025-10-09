#include <EEPROM.h>

#include <TFT_eSPI.h> // Graphics and font library for ST7735 driver chip
#include <SPI.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "time.h"
#include "RTClib.h"
 
RTC_DS3231 rtc;

#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */

RTC_DATA_ATTR int bootCount = 0;

TFT_eSPI tft = TFT_eSPI();  // Invoke library, pins defined in User_Setup.h

/*

TM struct:

int    tm_sec   seconds [0,61]
int    tm_min   minutes [0,59]
int    tm_hour  hour [0,23]
int    tm_mday  day of month [1,31]
int    tm_mon   month of year [0,11]
int    tm_year  years since 1900
int    tm_wday  day of week [0,6] (Sunday = 0)
int    tm_yday  day of year [0,365]
int    tm_isdst daylight savings flag

pins:

https://randomnerdtutorials.com/esp32-pinout-reference-gpios/#:~:text=The%20ESP32%20chip%20comes%20with,to%20use%20the%20ESP32%20GPIOs.

Sunrise / sunset times:

https://www.timeanddate.com/sun/usa/san-francisco

RTC

http://www.esp32learning.com/code/esp32-and-ds3231-rtc-example.php


TODOs:



 */

struct tm timeinfo;

DateTime now;

const float VOLTAGE_FACTOR=2.12f;

const byte RELAY_OPEN =HIGH;
const byte RELAY_CLOSE=LOW;

const byte BUTTON_1        = 0;
const byte BUTTON_2        = 35;
const byte VOLTAGE         = 38;

const byte COOP_DOOR_1_POS = 27;
const byte COOP_DOOR_1_NEG = 26;
const byte COOP_DOOR_2_POS = 25;
const byte COOP_DOOR_2_NEG = 33;
const byte RUN_DOOR_1_POS  = 32;
const byte RUN_DOOR_1_NEG  = 13;
const byte RUN_DOOR_2_POS  = 15;
const byte RUN_DOOR_2_NEG  = 2;
const byte LIGHT           = 17;
const byte AUX             = 12;

byte outputs[] = {
  COOP_DOOR_1_POS,
  COOP_DOOR_1_NEG,
  COOP_DOOR_2_POS,
  COOP_DOOR_2_NEG,
  RUN_DOOR_1_POS,
  RUN_DOOR_1_NEG,
  RUN_DOOR_2_POS,
  RUN_DOOR_2_NEG,
  LIGHT,
  AUX   
};

int NUM_OUTPUTS = sizeof(outputs) / sizeof(outputs[0]);

const int MODE_AUTO=0;
const int MODE_MANUAL=1;
const int MODE_TEST=2;

int mode=MODE_AUTO;

char* modeNames[3] {
  "Auto",
  "Manual",
  "Test"
};

const char* ssid       = "CH";
const char* password   = "xxxxx";

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -8*3600;
const int   daylightOffset_sec = 0*3600;

const int CLOSED = 0;
const int OPEN = 1;
const int OFF = 0;
const int ON = 1;

typedef struct {
  int pin1Pos;
  int pin1Neg;
  int pin2Pos;
  int pin2Neg;
  int countDown;
} actuator;

actuator doors[2]={
  {COOP_DOOR_1_POS,COOP_DOOR_1_NEG,COOP_DOOR_2_POS,COOP_DOOR_2_NEG,-1},
  {RUN_DOOR_1_POS, RUN_DOOR_1_NEG, RUN_DOOR_2_POS, RUN_DOOR_2_NEG, -1}  
};

const int COOP_ACTUATOR_IDX = 0;
const int RUN_ACTUATOR_IDX = 1;

typedef struct {
  int month;
  int day;
  int sunriseHour;
  int sunriseMin;
  int sunsetHour;
  int sunsetMin;
} sunTableEntry;

const int SUN_TABLE_ENTRIES=36;

sunTableEntry sunTable[SUN_TABLE_ENTRIES] = {
  { 1, 1, 7,25, 17, 0},
  { 1,10, 7,25, 17,10},
  { 1,20, 7,20, 17,20},

  { 2, 1, 7,13, 17,33},
  { 2,10, 7, 4, 17,43},
  { 2,20, 6,52, 17,54},

  { 3, 1, 6,50, 18, 3},  // { 3, 1, 6,40, 18, 3},
  { 3,10, 6,50, 18,12},  // { 3,10, 6,27, 18,12},
  { 3,20, 6,50, 18,21},  // { 3,20, 6,14, 18,21},  

  { 4, 1, 6,50, 18,32},  // { 4, 1, 5,54, 18,32},
  { 4,10, 6,50, 18,41},  // { 4,10, 5,41, 18,41},
  { 4,20, 6,50, 18,50},  // { 4,20, 5,27, 18,50},

  { 5, 1, 6,50, 19, 0},  // { 5, 1, 5,13, 19, 0},
  { 5,10, 6,50, 19, 8},  // { 5,10, 5, 4, 19, 8},
  { 5,20, 6,50, 19,17},  // { 5,20, 4,55, 19,17},

  { 6, 1, 6,50, 19,26},  // { 6, 1, 4,49, 19,26},
  { 6,10, 6,50, 19,31},  // { 6,10, 4,47, 19,31},
  { 6,20, 6,50, 19,34},  // { 6,20, 4,47, 19,34},

  { 7, 1, 6,50, 19,35},  // { 7, 1, 4,51, 19,35},
  { 7,10, 6,50, 19,33},  // { 7,10, 4,56, 19,33},
  { 7,20, 6,50, 19,27},  // { 7,20, 5, 3, 19,27},

  { 8, 1, 6,50, 19,17},  // { 8, 1, 5,13, 19,17},
  { 8,10, 6,50, 19, 8},  // { 8,10, 5,21, 19, 8},
  { 8,20, 6,50, 18,55},  // { 8,20, 5,29, 18,55},

  { 9, 1, 6,50, 18,38},  // { 9, 1, 5,40, 18,38},
  { 9,10, 6,50, 18,24},  // { 9,10, 5,47, 18,24},
  { 9,20, 6,50, 18, 9},  // { 9,20, 5,55, 18, 9},

  {10, 1, 6,50, 17,52},  // {10, 1, 6, 5, 17,52},
  {10,10, 6,50, 17,38},  // {10,10, 6,13, 17,38},
  {10,20, 6,50, 17,24},  // {10,20, 6,23, 17,24},

  {11, 1, 6,50, 17,10},  // {11, 1, 6,35, 17,10},
  {11,10, 6,50, 17, 1},  // {11,10, 6,44, 17, 1},
  {11,20, 6,55, 16,54},  
   
  {12, 1, 7, 5, 16,50},
  {12,10, 7,15, 16,50},
  {12,20, 7,20, 16,55}  
};

int findCurrentSunTableEntry() {
  int day=now.day();
  int month=now.month();
  int maxMatch=0;
  
  for(int i=0;i<SUN_TABLE_ENTRIES;i++) {
    if(month>=sunTable[i].month && day>=sunTable[i].day) {
      maxMatch=i;
    } 
  }
  return maxMatch;
}

const int SUNRISE = 0;
const int SUNSET  = 1;

typedef struct {
  int sunPos;
  int offset;
  int doorCoop;
  int doorRun;
  int light;
  char* name;
  boolean sleep;
} cronEntry;

cronEntry cron[8] = {
//  {"sunPos","offset","doorCoop" ,"doorRun" ,"light","name"           ,"sleep"}
    {-1      ,-1      ,-1         ,-1        ,-1     ,"NotInitialized" ,false  },
    {SUNRISE ,-28     ,CLOSED     ,CLOSED    ,ON     ,"Wakeup"         ,false  },
    {SUNRISE ,-25     ,OPEN       ,CLOSED    ,ON     ,"RunAM"          ,false  },
    {SUNRISE ,0       ,OPEN       ,OPEN      ,OFF    ,"Day"            ,true   },
    {SUNSET  ,-10     ,OPEN       ,OPEN      ,ON     ,"Dawn"           ,false  },
    {SUNSET  ,20      ,OPEN       ,CLOSED    ,ON     ,"RunPM"          ,false  },
    {SUNSET  ,35      ,OPEN       ,CLOSED    ,OFF    ,"Roost"          ,false  },
    {SUNSET  ,40      ,CLOSED     ,CLOSED    ,OFF    ,"Night"          ,true   }
};


int currentStateIdx=0;
int minStateIdx=1;
int maxStateIdx=7;

int sleepCountDown=0;

int testPos=0;

int loopCounter=0;

int delta=0;
boolean adjustedTime=false;
/*
void refreshTime() {
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  if(timeinfo.tm_sec%3==0)  {
    
    Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
/ *
    DateTime now = rtc.now();
     
    Serial.print(now.year(), DEC);
    Serial.print('/');
    Serial.print(now.month(), DEC);
    Serial.print('/');
    Serial.print(now.day(), DEC);
    Serial.print(' ');
    Serial.print(now.hour(), DEC);
    Serial.print(':');
    Serial.print(now.minute(), DEC);
    Serial.print(':');
    Serial.print(now.second(), DEC);
    Serial.println();
 * /   
  }
}

*/

void drawScreen()
{
  char buffer[50];
  sprintf(buffer,"%02d:%02d:%02d %-10s",now.hour(),now.minute(),now.second(),modeNames[mode]);
  //tft.fillScreen(TFT_BLACK);
  tft.drawString(buffer,0,0,4); 

  sprintf(buffer,"%s %d%s %20s",cron[currentStateIdx].name,delta,adjustedTime?"A":"N","");
  tft.drawString(buffer,0,40,4);

  int voltage=(int)analogRead(VOLTAGE)/VOLTAGE_FACTOR;
  if(voltage<100)voltage=0;
  sprintf(buffer,"%2d.%2d V    ",voltage/100,voltage%100);
  tft.drawString(buffer,100,80,4);
  
  for(int i=0;i<NUM_OUTPUTS;i++) {
    tft.fillRect(1+i*9, 90, 6, 6, digitalRead(outputs[i])?TFT_GREEN:TFT_RED);
  }
}

void setup()
{
  boolean gotInternetTime=false;
  
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK); // Note: the new fonts do not draw the background colour

  Serial.begin(115200);
  delay(1000); //Take some time to open up the Serial Monitor


  //Increment boot number and print it every reboot
  ++bootCount;
  Serial.println("Boot number: " + String(bootCount));  
  tft.drawString("Boot number: " + String(bootCount),0,0,4);
  delay(500); 

  //Print the wakeup reason for ESP32
  print_wakeup_reason();

  esp_sleep_enable_ext0_wakeup(GPIO_NUM_0,0); 
  
  //connect to WiFi
  Serial.printf("Connecting to %s ", ssid);
  tft.fillScreen(TFT_BLACK);
  tft.drawString("Connecting to "+String(ssid),0,0,4); 
  delay(500); 

  int wifiRetries=0;
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
      wifiRetries++;

      if(wifiRetries>120) {
        break;
      }
  }

  if(WiFi.status() == WL_CONNECTED)  {
    Serial.println(" CONNECTED");
    tft.fillScreen(TFT_BLACK);
    tft.drawString("Connected",0,0,4); 
    delay(500); 
  
    //init and get the time
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    if(!getLocalTime(&timeinfo)){
      Serial.println("Failed to obtain time");
    } else {
      Serial.println("Obtained time");
      gotInternetTime=true;
    }

    //disconnect WiFi as it's no longer needed
//    WiFi.disconnect(true);
//    WiFi.mode(WIFI_OFF);
  }

  for(int i=0;i<NUM_OUTPUTS;i++) {
    pinMode(outputs[i], OUTPUT);
    digitalWrite(outputs[i],RELAY_OPEN);
  }
  
  pinMode(BUTTON_1, INPUT);
  pinMode(BUTTON_2, INPUT);
  pinMode(VOLTAGE, INPUT);
  
  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");

    tft.fillScreen(TFT_BLACK);
    tft.drawString("Couldn't find RTC",0,0,4); 
    delay(500); 
    while (1);
  }
  tft.fillScreen(TFT_BLACK);
  tft.drawString("Found RTC",0,0,4); 
  delay(500); 

  now = rtc.now();
  delta=0;
  if(gotInternetTime) {
    delta=(now.hour()*60+now.minute()) - (timeinfo.tm_hour*60+timeinfo.tm_min);
    if(delta<0)delta=delta*-1;
  }

  if(delta>0 && (rtc.lostPower()||bootCount==1)) {   // rtc.lostPower()
    Serial.println("RTC time drift "+String(delta));

    tft.fillScreen(TFT_BLACK);
    tft.drawString("RTC time drift "+String(delta),0,0,4); 
    delay(500); 
    
    rtc.adjust(DateTime(timeinfo.tm_year+1900, 
                        timeinfo.tm_mon+1, 
                        timeinfo.tm_mday, 
                        timeinfo.tm_hour, 
                        timeinfo.tm_min, 
                        timeinfo.tm_sec));
    adjustedTime=true;
  }

  tft.fillScreen(TFT_BLACK);

  EEPROM.write(0, 9);
  EEPROM.commit();
}

void loop()
{
  now = rtc.now();
  
  loopCounter++;
  if(loopCounter>10000)loopCounter=0;
  
  drawScreen();
  delay(500);
  
  if(mode==MODE_MANUAL && !digitalRead(BUTTON_1)) {
    int stateIdx=currentStateIdx+1;
    if(stateIdx>maxStateIdx)stateIdx=minStateIdx;
    gotoState(stateIdx);
  }

  if(mode==MODE_AUTO) {
    int stateIdx=getStateFromTime();
    if(stateIdx!=currentStateIdx) {
      gotoState(stateIdx);
      if(cron[currentStateIdx].sleep) {
        sleepCountDown=2*120; // 2 min
      }
    } else if(cron[currentStateIdx].sleep) {
      sleepCountDown--;
      if(sleepCountDown<=0) {
        int minutes=getTimeToNextState();
        unsigned long long int timeToSleep=60l*minutes;
        Serial.println("Going to sleep for : " + String(minutes)); 
        esp_sleep_enable_timer_wakeup(timeToSleep * uS_TO_S_FACTOR);
        esp_deep_sleep_start();        
      }
    }
  }

  if(mode==MODE_TEST) {
    for(int i=0;i<NUM_OUTPUTS;i++) {
      digitalWrite(outputs[i],RELAY_OPEN);
    }
    digitalWrite(outputs[testPos],RELAY_CLOSE);
    testPos++;
    if(testPos>8) { 
      testPos=0;
      char buffer[50];
      int sunIndex=findCurrentSunTableEntry();
      sprintf(buffer,"%02d/%02d/%d Sunrise at %02d:%02d Sunset at %02d:%02d",now.month(),now.day(),now.year(),                               
                                                           sunTable[sunIndex].sunriseHour,
                                                           sunTable[sunIndex].sunriseMin,
                                                           sunTable[sunIndex].sunsetHour,
                                                           sunTable[sunIndex].sunsetMin);
      Serial.println(buffer);      
      for(int i=minStateIdx;i<=maxStateIdx;i++) {
        int cronTime=getAdjustedTime(i,sunIndex);
        
        sprintf(buffer,"%s at %02d:%02d",cron[i].name,cronTime/60,cronTime%60);
        Serial.println(buffer);
      }       
    }
  }

  if(!digitalRead(BUTTON_2)) {
    if(mode==MODE_MANUAL) {
      mode=MODE_AUTO;
      stopDoors(-1);
      currentStateIdx=0;
      Serial.println("Switched mode to automatic");
    } else if(mode==MODE_AUTO) {
      mode=MODE_TEST;
      stopDoors(-1);
      testPos=0;
      Serial.println("Switched mode to test");
    } else if(mode==MODE_TEST) {
      mode=MODE_MANUAL;
      stopDoors(-1);
      Serial.println("Switched mode to manual");
    }
  }  

  stopDoors(0);
}

void turnLight(int on) {
  digitalWrite(LIGHT, on==ON?RELAY_CLOSE:RELAY_OPEN);
}

void moveDoor(int actuator, int open) {
  digitalWrite(doors[actuator].pin1Pos, RELAY_OPEN);
  digitalWrite(doors[actuator].pin1Neg, RELAY_OPEN);
  digitalWrite(doors[actuator].pin2Pos, RELAY_OPEN);
  digitalWrite(doors[actuator].pin2Neg, RELAY_OPEN);  
  delay(200);

  //char buffer[50];
  //sprintf(buffer,"Closing relays on pins %d %d",open==OPEN?doors[actuator].pin1Pos:doors[actuator].pin1Neg,open==OPEN?doors[actuator].pin2Neg:doors[actuator].pin2Pos);
  //Serial.println(buffer);

  doors[actuator].countDown=60*2;  // 60 seconds
  digitalWrite(open==OPEN?doors[actuator].pin1Pos:doors[actuator].pin1Neg, RELAY_CLOSE);
  digitalWrite(open==OPEN?doors[actuator].pin2Neg:doors[actuator].pin2Pos, RELAY_CLOSE);
}

int getTimeToNextState() {
  int nextStateIdx=currentStateIdx==maxStateIdx?minStateIdx:currentStateIdx+1;

  //int nextTime=cron[nextStateIdx].hour*60+cron[nextStateIdx].min;
  int sunIndex=findCurrentSunTableEntry();

  //Serial.println("Suntable index: "+String(sunIndex));
  
  int nextTime=getAdjustedTime(nextStateIdx,sunIndex);
  
  int currentTime=now.hour()*60+now.minute();
  int delta=nextTime>=currentTime?(nextTime-currentTime):24*60-currentTime+nextTime;
  return delta;
}


int getStateFromTime() {
  // Default to last cron entry, if no matches with an earlier time are found, the last one is the correct one 
  int cronIdx=maxStateIdx;
  int currentTime=now.hour()*60+now.minute();
  int sunIndex=findCurrentSunTableEntry();
  
  for(int i=minStateIdx;i<=maxStateIdx;i++) {
    int cronTime=getAdjustedTime(i,sunIndex);
    //int cronTime=cron[i].hour*60+cron[i].min;
    if(cronTime<=currentTime)cronIdx=i;
  }
  return cronIdx;
}

int getAdjustedTime(int cronIndex,int sunIndex) {
  int minOfDay=0;
  if(cron[cronIndex].sunPos==SUNRISE) {
    minOfDay=sunTable[sunIndex].sunriseHour*60+sunTable[sunIndex].sunriseMin+cron[cronIndex].offset;
    //Serial.println("Sunrise min of day: "+String(minOfDay) +" "+String(cron[cronIndex].offset));  
  } else {
    minOfDay=sunTable[sunIndex].sunsetHour*60+sunTable[sunIndex].sunsetMin+cron[cronIndex].offset;
    //Serial.println("Sunset min of day: "+String(minOfDay) +" "+String(cron[cronIndex].offset));
  }
  return minOfDay;
}


void stopDoors(int mode) {
  for(int i=0;i<2;i++) {
    if(doors[i].countDown==0 || mode==-1)  {
      doors[i].countDown=-1;
      digitalWrite(doors[i].pin1Pos, RELAY_OPEN);
      digitalWrite(doors[i].pin1Neg, RELAY_OPEN);
      digitalWrite(doors[i].pin2Pos, RELAY_OPEN);
      digitalWrite(doors[i].pin2Neg, RELAY_OPEN);   
    } else if(doors[i].countDown>0) {
       doors[i].countDown--;
    }  
  }
}

void gotoState(int stateIdx) {

  char buffer[50];
  sprintf(buffer,"Switching state to %d",stateIdx);
  Serial.println(buffer);

  if(cron[stateIdx].doorCoop!=cron[currentStateIdx].doorCoop) {
    moveDoor(COOP_ACTUATOR_IDX, cron[stateIdx].doorCoop);
  }
  if(cron[stateIdx].doorRun!=cron[currentStateIdx].doorRun) {
    moveDoor(RUN_ACTUATOR_IDX, cron[stateIdx].doorRun);
  }   
  if(cron[stateIdx].light!=cron[currentStateIdx].light) {
    turnLight(cron[stateIdx].light);
  }
  
  currentStateIdx=stateIdx;    

  sendLog(); 
}

void print_wakeup_reason(){
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }
}



void sendLog() {
  if(WiFi.status()== WL_CONNECTED){
    HTTPClient http;
    int voltage=(int)analogRead(VOLTAGE)/VOLTAGE_FACTOR;
    if(voltage<100)voltage=0;
    char buffer[200];
    
    sprintf(buffer,"https://12bx3x78jb.execute-api.us-west-2.amazonaws.com/default/cronLogger?action=LOG&msg=%04d-%02d-%02d%%20%02d%%3A%02d%%3A%02d%%20%s%%20%02d.%02d%%20V",
          now.year(),now.month(),now.day(),now.hour(),now.minute(),now.second(),cron[currentStateIdx].name,voltage/100,voltage%100);
    Serial.println(buffer);
    http.begin(buffer);
      
      // Send HTTP GET request
      int httpResponseCode = http.GET();
      
      if (httpResponseCode>0) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        String payload = http.getString();
        Serial.println(payload);
      }
      else {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
      }
      // Free resources
      http.end();
    }
    else {
      //Serial.println("WiFi Disconnected");
    }

}
