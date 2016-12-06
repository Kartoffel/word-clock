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

#define MIN_BRIGHTNESS  65
#define MAX_BRIGHTNESS  200
int     BRIGHTNESS      = MIN_BRIGHTNESS;
int     dayHour         = 8; // Start increasing brightness
int     nightHour       = 22; // Start decreasing brightness

#define SYNC_INTERVAL   1200

#define NUM_LEDS        95
#define DATA_PIN        4 // D2 Pin on Wemos mini

const int   timeZone        = 1;     // Central European Time
bool        autoDST         = true;

IPAddress   timeServerIP; // time.nist.gov NTP server address
const char* ntpServerName   = "nl.pool.ntp.org";
const int   NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte        packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets
WiFiUDP     Udp;
unsigned int localPort = 8888;

time_t getNtpTime();
void sendNTPpacket(IPAddress &address);

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
    Serial.println("Starting UDP");
    Udp.begin(localPort);
    LEDS.addLeds<NEOPIXEL,DATA_PIN>(leds,NUM_LEDS);
    LEDS.setBrightness(87);
    rainbow();
    LEDS.setBrightness(MIN_BRIGHTNESS);
    for(int i=0;i<NUM_LEDS;i++) {
        targetlevels[i] = 0;
        currentlevels[i] = 0;
        leds[i] = CRGB::Black;
    }

    FastLED.show();

    setSyncProvider(getNtpTime);
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

int timeBrightness() {
    if (hour() > dayHour && hour() < nightHour) {
        return MAX_BRIGHTNESS;
    } else if (hour() < dayHour || hour() > nightHour) {
        return MIN_BRIGHTNESS;
    } else if (hour() == dayHour) {
        return constrain(
                map(minute(), 0, 29, MIN_BRIGHTNESS, MAX_BRIGHTNESS),
                MIN_BRIGHTNESS, MAX_BRIGHTNESS);
    } else if (hour() == nightHour) {
        return constrain(
                map(minute(), 0, 29, MAX_BRIGHTNESS, MIN_BRIGHTNESS),
                MIN_BRIGHTNESS, MAX_BRIGHTNESS);
    }
}

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

    BRIGHTNESS = timeBrightness();

    // if not connected, then show waiting animation
    if(timeStatus() == timeNotSet) {
        // show initialisation animation
        Serial.println("time not yet known");
        for(int i=0;i<NUM_LEDS;++i){ //blank rest
            leds[i] = CRGB::Black;
        }

        float phase = ((float)(millis()%2000)) / 1000;
        if(phase > 1) phase = 2.0f-phase;
        for(int i=0;i<4;++i){  // the scanner moves from 0 to(inc) 5, but only 1..4 are actually shown on the four leds
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

/*-------- NTP code ----------*/

time_t getNtpTime()
{
    IPAddress ntpServerIP; // NTP server's ip address

    while (Udp.parsePacket() > 0) ; // discard any previously received packets
    Serial.println("Transmit NTP Request");
    // get a random server from the pool
    WiFi.hostByName(ntpServerName, ntpServerIP);
    Serial.print(ntpServerName);
    Serial.print(": ");
    Serial.println(ntpServerIP);
    sendNTPpacket(ntpServerIP);
    uint32_t beginWait = millis();
    while (millis() - beginWait < 1500) {
        int size = Udp.parsePacket();
        if (size >= NTP_PACKET_SIZE) {
            Serial.println("Receive NTP Response");
            Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
            unsigned long secsSince1900;
            // convert four bytes starting at location 40 to a long integer
            secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
            secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
            secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
            secsSince1900 |= (unsigned long)packetBuffer[43];
            // New time in seconds since Jan 1, 1970
            unsigned long newTime = secsSince1900 - 2208988800UL +
                timeZone * SECS_PER_HOUR;

            // Auto DST
            if (autoDST) {
                if (isDSTSwitchDay(day(newTime), month(newTime), year(newTime))) {
                    if (month(newTime) == 3 && hour(newTime) >= 2) {
                        newTime += SECS_PER_HOUR;
                    } else if (month(newTime) == 10 && hour(newTime) < 2) {
                        newTime += SECS_PER_HOUR;
                    }
                } else if (isDST(day(newTime), month(newTime), year(newTime))) {
                    newTime += SECS_PER_HOUR;
                }
            }

            setSyncInterval(SYNC_INTERVAL);
            return newTime;
        }
    }
    Serial.println("No NTP Response :-(");
    // Retry soon
    setSyncInterval(10);
    return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
    // set all bytes in the buffer to 0
    memset(packetBuffer, 0, NTP_PACKET_SIZE);
    // Initialize values needed to form NTP request
    // (see URL above for details on the packets)
    packetBuffer[0] = 0b11100011;   // LI, Version, Mode
    packetBuffer[1] = 0;     // Stratum, or type of clock
    packetBuffer[2] = 6;     // Polling Interval
    packetBuffer[3] = 0xEC;  // Peer Clock Precision
    // 8 bytes of zero for Root Delay & Root Dispersion
    packetBuffer[12] = 49;
    packetBuffer[13] = 0x4E;
    packetBuffer[14] = 49;
    packetBuffer[15] = 52;
    // all NTP fields have been given values, now
    // you can send a packet requesting a timestamp:
    Udp.beginPacket(address, 123); //NTP requests are to port 123
    Udp.write(packetBuffer, NTP_PACKET_SIZE);
    Udp.endPacket();
}

// Check if Daylight saving time (DST) applies
// Northern Hemisphere - +1 hour between March and October
bool isDST(int d, int m, int y){
    bool dst = false;
    dst = (m > 3 && m < 10); // October-March

    if (m == 3){
        // Last sunday of March
        dst = (d >= ((31 - (5 * y /4 + 4) % 7)));
    }else if (m == 10){
        // Last sunday of October
        dst = (d < ((31 - (5 * y /4 + 1) % 7)));
    }

    return dst;
}

bool isDSTSwitchDay(int d, int m, int y){
    bool dst = false;
    if (m == 3){
        // Last sunday of March
        dst = (d == ((31 - (5 * y /4 + 4) % 7)));
    }else if (m == 10){
        // Last sunday of October
        dst = (d == ((31 - (5 * y /4 + 1) % 7)));
    }
    return dst;
}
