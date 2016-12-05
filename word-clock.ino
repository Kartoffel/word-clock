#define FASTLED_ESP8266_RAW_PIN_ORDER
#define FASTLED_ALLOW_INTERRUPTS 0
#include "FastLED.h"
#include <FS.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <TimeLib.h>
#include <ESP8266HTTPClient.h>
#include <vector>

#define R_VALUE         255
#define G_VALUE         255
#define B_VALUE         250

#define BRIGHTNESS      200

#define SYNC_INTERVAL   3000

#define NUM_LEDS        95
#define DATA_PIN        4 // D2 Pin on Wemos mini

time_t          fetchTime();
unsigned long   lastdisplayupdate   = 0;

CRGB leds[NUM_LEDS];

uint8_t targetlevels[NUM_LEDS];
uint8_t currentlevels[NUM_LEDS];

void setup() {
    Serial.begin(115200);

    WiFiManager wifiManager;

    ArduinoOTA.onStart([]() {
            Serial.println("Start");
            });
    ArduinoOTA.onEnd([]() {
            Serial.println("\nEnd");
            });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
            Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
            });
    ArduinoOTA.onError([](ota_error_t error) {
            Serial.printf("Error[%u]: ", error);
            if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
            else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
            else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
            else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
            else if (error == OTA_END_ERROR) Serial.println("End Failed");
            });
    ArduinoOTA.begin();

    String ssid = "WordClock-" + String(ESP.getChipId());
    wifiManager.autoConnect(ssid.c_str());

    Serial.println("Connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    LEDS.addLeds<NEOPIXEL,DATA_PIN>(leds,NUM_LEDS);
    LEDS.setBrightness(87);
    rainbow();
    LEDS.setBrightness(BRIGHTNESS);
    for(int i=0;i<NUM_LEDS;i++) {
        targetlevels[i] = 0;
        currentlevels[i] = 0;
        leds[i] = CRGB::Black;
    }

    FastLED.show();

    setSyncProvider(fetchTime);
    setSyncInterval(SYNC_INTERVAL);
}

void rainbow() {
    uint8_t gHue = 0;
    while (gHue < 255) {
        EVERY_N_MILLISECONDS(20) {gHue++;}
        fill_rainbow(leds, NUM_LEDS, gHue, 1);
        FastLED.delay(1000/30); // 30FPS
    }
}


time_t fetchTime() {
    HTTPClient http;

    Serial.print("[HTTP] begin...\n");
    http.begin("http://timezone.1sand0s.nl/localtime"); //HTTP
    Serial.print("[HTTP] GET...\n");
    // start connection and send HTTP header
    int httpCode = http.GET();
    String payload;
    // httpCode will be negative on error
    if(httpCode > 0) {
        // HTTP header has been send and Server response header has been handled
        Serial.printf("[HTTP] GET... code: %d\n", httpCode);

        // file found at server
        if(httpCode == HTTP_CODE_OK) {
            payload = http.getString();
            Serial.println(payload);
        }
    } else {
        Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
        return 0;
    }
    http.end();

    StaticJsonBuffer<600> jsonBuffer;

    JsonObject& root = jsonBuffer.parseObject(payload);
    if (!root.success())
    {
        Serial.println("parseObject() failed");
    }

    tmElements_t tm;
    tm.Second = root["seconds"];
    tm.Minute = root["minutes"];
    tm.Hour   = root["hours"];

    Serial.print("setting time to ");
    tm.Year = 2016-1970;
    tm.Day = 15;
    tm.Month=11;
    Serial.print(tm.Hour);
    Serial.print(":");
    Serial.print(tm.Minute);
    Serial.print(":");
    Serial.print(tm.Second);
    Serial.println();

    time_t res = makeTime(tm);
    // unsigned long test = makeTime(tm);
    Serial.print("systemtime is ");
    Serial.println(res);
    return res;
}





/*

   HET IS X UUR
   HET IS VIJF OVER X
   HET IS TIEN OVER X
   HET IS KWART OVER X
   HET IS TIEN VOOR HALF (X+1)
   HET IS VIJF VOOR HALF (X+1)
   HET IS HALF (X+1)
   HET IS VIJF OVER HALF (X+1)
   HET IS TIEN OVER HALF (X+1)
   HET IS KWART VOOR (X+1)
   HET IS TIEN VOOR (X+1)
   HET IS VIJF VOOR (X+1)

   HET IS (X+1) UUR
   ...

*/

#ifdef WEATHERCLOCK
#define LIGHT   0
#define HEAVY   1
#define RAIN    2
#define NO      3
#define FOR     4
#define IN      5
#define TEN     6
#define FIFTEEN   7
#define TWENTY    8
#define RAIN2   9
#define EXPECTED  10
#define AN      11
#define NEXT    12
#define THIRTY    13
#define FOURTY    14
#define HOUR    15
#define FIFTY   16
#define FIVE    17
#define MINUTES   18

// letters for weatherclock
// LIGHTHEAVY
// RAINOFORIN
// TENFIFTEEN
// TWENTYRAIN
// EXPECTED
// ANEXTHIRTY
// FOURTYHOUR
// FIFTY FIVE
//  MINUTES


std::vector<std::vector<int>> weatherwords = {
    {84,85,86,87,88},       // LIGHT
    {89,90,91,92,93},       // HEAVY
    {83,82,81,80},        // RAIN
    {80,79},          // NO
    {78,77,76},         // FOR
    {75,74},          // IN
    {64,65,66},         // TEN
    {67,68,69,70,71,72,73},   // FIFTEEN
    {63,62,61,60,59,58},    // TWENTY
    {57,56,55,54},        // RAIN2
    {44,45,46,47,48,49,50,51},  // EXPECTED
    {43,42},          // AN
    {42,41,40,39},        // NEXT
    {39,38,37,36,35,34},    // THIRTY
    {24,25,26,27,28,29},    // FOURTY
    {30,31,32,33},        // HOUR
    {23,22,21,20,19},       // FIFTY
    {17,16,15,14},        // FIVE
    {5,6,7,8,9,10,11}     // MINUTES
};

#else

#define HETIS 0
#define VIJF  13
#define TIEN  14
#define KWART 15
#define VOOR  16
#define OVER  17
#define HALF  18
#define UUR   19

std::vector<std::vector<int>> ledsbyword = {
    {84,85,86, 89,90}, // HET IS
    {4,5,6},            // een
    {47,48,49,50},      // twee
    {43,42,41,40},      // drie
    {23,22,21,20},      // vier
    {7,8,9,10},         // vijf
    {51,52,53},         // zes
    {25,26,27,28,29},   // zeven
    {44,45,46,47},      // acht
    {29,30,31,32,33},   // negen
    {37,36,35,34},      // tien
    {40,39,38},         // elf
    {19,18,17,16,15,14},// twaalf
    {82,81,80,79},    // VIJF
    {78,77,76,75},    // TIEN
    {64,65,66,67,68}, // KWART
    {63,62,61,60},    // VOOR
    {70,71,72,73},    // OVER
    {58,57,56,55},    // HALF
    {11,12,13}      // UUR
};

#endif


void loop() {
    // put your main code here, to run repeatedly:
    ArduinoOTA.handle();
    //  Serial.println("loop");

    // only update clock every 50ms
    if(millis()-lastdisplayupdate > 50) {
        lastdisplayupdate = millis();
    }else{
        return;
    }

    // if not connected, then show waiting animation
    if(timeStatus() == timeNotSet) {
        // show initialisation animation
        Serial.println("time not yet known");
        for(int i=0;i<NUM_LEDS;++i){ //blank rest
            leds[i] = CRGB::Black;
        }

        float phase = ((float)(millis()%2000)) / 1000;
        if(phase > 1) phase = 2.0f-phase;
        for(int i=1;i<5;++i){  // the scanner moves from 0 to(inc) 5, but only 1..4 are actually shown on the four leds
            float intensity = abs((float)(i-1)/3-phase);
            intensity = sqrt(intensity);
            leds[i] = CRGB(255-(255*intensity),0,0);
        }
        FastLED.show();
        return;
    }

    time_t t = now();

    // calculate target brightnesses:
    int current_hourword = hour();
    if(current_hourword>12) current_hourword = current_hourword - 12; // 12 hour clock, where 12 stays 12 and 13 becomes one
    if(current_hourword==0) current_hourword = 12;            // 0 is also called 12

    int next_hourword = hour()+1;
    if(next_hourword>12) next_hourword = next_hourword - 12;      // 12 hour clock, where 12 stays 12 and 13 becomes one
    if(next_hourword==0) next_hourword = 12;              // 0 is also called 12

    for(int i=0;i<NUM_LEDS;++i) {
        targetlevels[i] = 0;
    }

    for(int l : ledsbyword[HETIS]) { targetlevels[l] = 255; }
    switch((minute()%60)/5) {
        case 0:
            for(int l : ledsbyword[current_hourword])   { targetlevels[l] = 255; }
            for(int l : ledsbyword[UUR])        { targetlevels[l] = 255; }
            break;
        case 1:
            for(int l : ledsbyword[VIJF])         { targetlevels[l] = 255; }  
            for(int l : ledsbyword[OVER])         { targetlevels[l] = 255; }
            for(int l : ledsbyword[current_hourword]) { targetlevels[l] = 255; }
            break;
        case 2:
            for(int l : ledsbyword[TIEN])         { targetlevels[l] = 255; }
            for(int l : ledsbyword[OVER])         { targetlevels[l] = 255; }
            for(int l : ledsbyword[current_hourword])   { targetlevels[l] = 255; }
            break;
        case 3:
            for(int l : ledsbyword[KWART])        { targetlevels[l] = 255; }
            for(int l : ledsbyword[OVER])         { targetlevels[l] = 255; }
            for(int l : ledsbyword[current_hourword])   { targetlevels[l] = 255; }
            break;
        case 4:
            for(int l : ledsbyword[TIEN])         { targetlevels[l] = 255; }
            for(int l : ledsbyword[VOOR])         { targetlevels[l] = 255; }
            for(int l : ledsbyword[HALF])         { targetlevels[l] = 255; }
            for(int l : ledsbyword[next_hourword])    { targetlevels[l] = 255; }
            break;
        case 5:
            for(int l : ledsbyword[VIJF])         { targetlevels[l] = 255; }
            for(int l : ledsbyword[VOOR])         { targetlevels[l] = 255; }
            for(int l : ledsbyword[HALF])         { targetlevels[l] = 255; }
            for(int l : ledsbyword[next_hourword])    { targetlevels[l] = 255; }
            break;
        case 6:
            for(int l : ledsbyword[HALF])         { targetlevels[l] = 255; }
            for(int l : ledsbyword[next_hourword])    { targetlevels[l] = 255; }
            break;
        case 7:
            for(int l : ledsbyword[VIJF])         { targetlevels[l] = 255; }
            for(int l : ledsbyword[OVER])         { targetlevels[l] = 255; }
            for(int l : ledsbyword[HALF])         { targetlevels[l] = 255; }
            for(int l : ledsbyword[next_hourword])    { targetlevels[l] = 255; }
            break;
        case 8:
            for(int l : ledsbyword[TIEN])         { targetlevels[l] = 255; }
            for(int l : ledsbyword[OVER])         { targetlevels[l] = 255; }
            for(int l : ledsbyword[HALF])         { targetlevels[l] = 255; }
            for(int l : ledsbyword[next_hourword])    { targetlevels[l] = 255; }
            break;
        case 9:
            for(int l : ledsbyword[KWART])        { targetlevels[l] = 255; }
            for(int l : ledsbyword[VOOR])         { targetlevels[l] = 255; }
            for(int l : ledsbyword[next_hourword])    { targetlevels[l] = 255; }
            break;
        case 10:
            for(int l : ledsbyword[TIEN])         { targetlevels[l] = 255; }
            for(int l : ledsbyword[VOOR])         { targetlevels[l] = 255; }
            for(int l : ledsbyword[next_hourword])    { targetlevels[l] = 255; }
            break;
        case 11:
            for(int l : ledsbyword[VIJF])         { targetlevels[l] = 255; }
            for(int l : ledsbyword[VOOR])         { targetlevels[l] = 255; }
            for(int l : ledsbyword[next_hourword])    { targetlevels[l] = 255; }
            break;
    }

    // the minute leds at the bottom:
    for(int i=4-(minute()%5);i<4;++i) {
        targetlevels[i] = 255;
    }

    int speed = 4;


    // move current brightness towards target brightness:
    for(int i=0;i<NUM_LEDS;++i) {
        if(currentlevels[i] < targetlevels[i]) {
            currentlevels[i] = std::min(BRIGHTNESS,currentlevels[i]+speed);
        }
        if(currentlevels[i] > targetlevels[i]) {
            currentlevels[i] = std::max(0,currentlevels[i]-speed);
        }

        // output the value to led: according to the function x^2/255 to compensate for the perceived brightness of leds which is not linear
        leds[i] = CRGB(
                currentlevels[i]*currentlevels[i]*R_VALUE/65025,
                currentlevels[i]*currentlevels[i]*G_VALUE/65025,
                currentlevels[i]*currentlevels[i]*B_VALUE/65025);
    }

    // Update LEDs
    FastLED.show();
}
