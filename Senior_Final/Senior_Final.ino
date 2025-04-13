#include "pins.h"
#include <SPI.h>
#include <Arduino.h>
#include <xpt2046.h>
#include <WiFi.h>
#include "time.h"
#include <TinyGPS++.h>
#include <HardwareSerial.h>
#include <PubSubClient.h>

#if __has_include("data.h")
#include "data.h"
#define USE_CALIBRATION_DATA 1
#endif

#define RXPin (44)
#define TXPin (43)

#include "TFT_eSPI.h"
#define USE_TFT_eSPI 1


XPT2046 touch = XPT2046(SPI, TOUCHSCREEN_CS_PIN, TOUCHSCREEN_IRQ_PIN);

String ssid     = "";
String password = "";
float latitude;
float longitude;
float cube_div;
float cube_a;
float cube_b;
float cube_c;
float cube_d;
float cube_p;
float cube_q;
float cube_r;
float cube_ans;
float cube_ans1;
float cube_ans2;
float cube_num;
float intense1;
float intense2;
String strlati;
String strlongi;
const int led = 17;
const int ldr = 18;
int lightmin;
int lightmax;
int set;
int setlim = 20;
int lightsum;
int lightcur;
float conoutput;
static const uint32_t GPSBaud = 9600;
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 25200;
const int   daylightOffset_sec = 0;
String curday = "";
String curmonth = "";
String curyear = "";
String curhour = "";
String curmin = "";
String currentdate = "";
String sendmes = "";
TinyGPSPlus gps;
HardwareSerial ss(2);

// MQTT Broker
const char *mqtt_broker = "broker.emqx.io";  // MQTT Server
const int mqtt_port = 1883;  // MQTT Port
const char *topic = "nowtemp101";   // key to get
const char *mqtt_username = "emqx";
const char *mqtt_password = "public";
const int Max_string = 50;
char mqtt_payload[Max_string] = "";

WiFiClient espClient;
PubSubClient client(espClient);

#if USE_CALIBRATION_DATA
touch_calibration_t calibration_data[4];
#endif


TFT_eSPI tft = TFT_eSPI();


void setBrightness(uint8_t value)
{
    static uint8_t steps = 16;
    static uint8_t _brightness = 0;

    if (_brightness == value) {
        return;
    }

    if (value > 16) {
        value = 16;
    }
    if (value == 0) {
        digitalWrite(BK_LIGHT_PIN, 0);
        delay(3);
        _brightness = 0;
        return;
    }
    if (_brightness == 0) {
        digitalWrite(BK_LIGHT_PIN, 1);
        _brightness = steps;
        delayMicroseconds(30);
    }
    int from = steps - _brightness;
    int to = steps - value;
    int num = (steps + to - from) % steps;
    for (int i = 0; i < num; i++) {
        digitalWrite(BK_LIGHT_PIN, 0);
        digitalWrite(BK_LIGHT_PIN, 1);
    }
    _brightness = value;
}

void printLocalTime(){
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%B %d %Y %H:%M:%S");
  curday = timeinfo.tm_mday;
  curyear = timeinfo.tm_year+1900;
  curmonth = timeinfo.tm_mon+1;
  curhour = timeinfo.tm_hour;
  curmin = timeinfo.tm_min;
  if (curhour.length() == 1){
      curhour = "0"+curhour;
  }
  if (curmin.length() == 1){
      curmin = "0"+curmin;
  }
  currentdate = curday+"/"+curmonth+"/"+curyear+","+curhour+":"+curmin;
}

void concentesttemt(){
  lightsum = 0;
  set = 1;
  lightmin = 99999;
  lightmax = 0;
  digitalWrite(led, HIGH);
  delay(1000);
  while(set<= setlim){
    lightcur = analogRead(ldr);
    lightsum += lightcur;
    if(lightcur < lightmin){
      lightmin = lightcur;
    }
    if(lightcur > lightmax){
      lightmax = lightcur;
    }
    set += 1;
    delay(50);
  }
  digitalWrite(led, LOW);
  conoutput = (lightsum-lightmin-lightmax)/(setlim-2); 
  delay(250);
}

void cubicsolve(){
  cube_num = 0;
  cube_div = intense2/intense1;
  Serial.print("cube_div is ");
  Serial.println(cube_div,6);
  cube_a = -0.0084;
  cube_b = 0.1031;
  cube_c = -0.4707;
  cube_d = 0.9434 - cube_div;
  cube_p = -(cube_b)/(3*cube_a);
  Serial.print("cube_p is ");
  Serial.println(cube_p,6);
  cube_q = pow(cube_p,3)+(cube_b*cube_c-(3*cube_a*cube_d))/(6*pow(cube_a,2));
  Serial.print("cube_q is ");
  Serial.println(cube_q,6);
  cube_r = cube_c/(3*cube_a);
  Serial.print("cube_r is ");
  Serial.println(cube_r,6);
  cube_num = cube_r-(cube_p*cube_p);
  cube_num = pow(cube_num,3);
  cube_num = (cube_q*cube_q)+cube_num;
  cube_num = sqrt(cube_num);
  cube_num = cube_q+cube_num;
  cube_ans1 = cbrt(cube_num);
  cube_num = 0;
  cube_num = cube_r-(cube_p*cube_p);
  cube_num = pow(cube_num,3);
  cube_num = (cube_q*cube_q)+cube_num;
  cube_num = sqrt(cube_num);
  cube_num = cube_q-cube_num;
  cube_ans2 = cbrt(cube_num);
  Serial.print("cube ans 1 = ");
  Serial.println(cube_ans1,6);
  Serial.print("cube ans 2 = ");
  Serial.println(cube_ans2,6);
  cube_ans = cube_ans1+cube_ans2+cube_p;
  cube_ans = abs(cube_ans);
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("outTopic", "hello world");
      // ... and resubscribe
      client.subscribe("inTopic");
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
    Serial.begin(115200);
    delay(1000);
    pinMode(led,OUTPUT);
    pinMode(10, OUTPUT);
    digitalWrite(10, HIGH);
    pinMode(14, OUTPUT);
    digitalWrite(14, HIGH);
    pinMode(0, OUTPUT);
    digitalWrite(0, HIGH);

#if USE_CALIBRATION_DATA
    data_init();
    data_read(calibration_data);
#endif

    SPI.begin(TOUCHSCREEN_SCLK_PIN, TOUCHSCREEN_MISO_PIN, TOUCHSCREEN_MOSI_PIN);
    touch.begin(240, 320);
#if USE_CALIBRATION_DATA
    touch.setCal(calibration_data[0].rawX, calibration_data[2].rawX, calibration_data[0].rawY, calibration_data[2].rawY, 240, 320); // Raw xmin, xmax, ymin, ymax, width, height
#else
    touch.setCal(1788, 285, 1877, 311, 240, 320); // Raw xmin, xmax, ymin, ymax, width, height
    Serial.println("Use default calibration data");
#endif
    touch.setRotation(1);


    ss.begin(GPSBaud, SERIAL_8N1, RXPin, TXPin, false);
    tft.begin();
    tft.setRotation(1);

    // Set backlight level, range 0 ~ 16
    setBrightness(50);

    delay(3000);
    
}


float concentration;

String s;
int A;
int B;
String text = "";
bool shift = false;
bool number = false;
bool keyboard = false;
bool connectwifi = false;
int state = -1;
int numcount = 0;
int gpscount = 0;
String wifi = "";
String pass = "";
String concenstr;

String x1y1;
String x2y1;
String x3y1;
String x4y1;
String x5y1;
String x6y1;
String x7y1;
String x8y1;
String x9y1;
String x10y1;

String x1y2;
String x2y2;
String x3y2;
String x4y2;
String x5y2;
String x6y2;
String x7y2;
String x8y2;
String x9y2;
String x10y2;

String x1y3;
String x2y3;
String x3y3;
String x4y3;
String x5y3;
String x6y3;
String x7y3;
String x8y3;
String x9y3;
String x10y3;

String x1y4;
String x2y4;
String x3y4;
String x4y4;
String x5y4;
String x6y4;

bool presstest1 = false;
bool pressonline = false;
bool pressoffline = false;
bool online = false;

bool pressx1y1 = false;
bool pressx2y1 = false;
bool pressx3y1 = false;
bool pressx4y1 = false;
bool pressx5y1 = false;
bool pressx6y1 = false;
bool pressx7y1 = false;
bool pressx8y1 = false;
bool pressx9y1 = false;
bool pressx10y1 = false;

bool pressx1y2 = false;
bool pressx2y2 = false;
bool pressx3y2 = false;
bool pressx4y2 = false;
bool pressx5y2 = false;
bool pressx6y2 = false;
bool pressx7y2 = false;
bool pressx8y2 = false;
bool pressx9y2 = false;
bool pressx10y2 = false;

bool pressx1y3 = false;
bool pressx2y3 = false;
bool pressx3y3 = false;
bool pressx4y3 = false;
bool pressx5y3 = false;
bool pressx6y3 = false;
bool pressx7y3 = false;
bool pressx8y3 = false;
bool pressx9y3 = false;
bool pressx10y3 = false;

bool pressx1y4 = false;
bool pressx2y4 = false;
bool pressx3y4 = false;
bool pressx4y4 = false;
bool pressx5y4 = false;
bool pressx6y4 = false;


void loop() {
  tft.setTextSize(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  if (touch.pressed()) {
        s = "X: ";
        s += touch.X();
        s += " Y: ";
        s += touch.Y();
  #if USE_TFT_eSPI
  #endif
    } 
  if (touch.pressed()){
    A = touch.X();
    B = touch.Y();
    }
  if (state == -1){
    tft.drawString("ONLINE OR OFFLINE", 90, 40,2 );
    tft.drawRect(35, 110, 90, 50, TFT_WHITE);
    tft.drawString("ONLINE", 60, 125,2 );
    tft.drawRect(195, 110, 90, 50, TFT_WHITE);
    tft.drawString("OFFLINE", 215, 125,2 );

    if ((A >= 25) && (A < 120) && (B >= 105) && (B < 165)){
      pressonline = true;
    }
    else{
      if (pressonline){
        online = true;
        state = 1;
        if(wifi.length() != 0){
          text = wifi;
        }
        else{
          text = "";
        }
        pressonline = false;
      }
    }

    if ((A >= 200) && (A < 295) && (B >= 105) && (B < 165)){
      pressoffline = true;
    }
    else {
      if (pressoffline){
        online = false;
        state = 9;
        pressoffline = false;
      }
    }
  }
  if ((state == 1)||(state == 2)){
    keyboard = true;
  }
  else{
    keyboard = false;
  }
  if (keyboard){
    tft.drawRect(0, 120, 32, 30, TFT_WHITE);
    tft.drawRect(32, 120, 32, 30, TFT_WHITE);
    tft.drawRect(64, 120, 32, 30, TFT_WHITE);
    tft.drawRect(96, 120, 32, 30, TFT_WHITE);
    tft.drawRect(128, 120, 32, 30, TFT_WHITE);
    tft.drawRect(160, 120, 32, 30, TFT_WHITE);
    tft.drawRect(192, 120, 32, 30, TFT_WHITE);
    tft.drawRect(224, 120, 32, 30, TFT_WHITE);
    tft.drawRect(256, 120, 32, 30, TFT_WHITE);
    tft.drawRect(288, 120, 32, 30, TFT_WHITE);

    tft.drawRect(0, 150, 32, 30, TFT_WHITE);
    tft.drawRect(32, 150, 32, 30, TFT_WHITE);
    tft.drawRect(64, 150, 32, 30, TFT_WHITE);
    tft.drawRect(96, 150, 32, 30, TFT_WHITE);
    tft.drawRect(128, 150, 32, 30, TFT_WHITE);
    tft.drawRect(160, 150, 32, 30, TFT_WHITE);
    tft.drawRect(192, 150, 32, 30, TFT_WHITE);
    tft.drawRect(224, 150, 32, 30, TFT_WHITE);
    tft.drawRect(256, 150, 32, 30, TFT_WHITE);
    tft.drawRect(288, 150, 32, 30, TFT_WHITE);

    tft.drawRect(0, 180, 32, 30, TFT_WHITE);
    tft.drawRect(32, 180, 32, 30, TFT_WHITE);
    tft.drawRect(64, 180, 32, 30, TFT_WHITE);
    tft.drawRect(96, 180, 32, 30, TFT_WHITE);
    tft.drawRect(128, 180, 32, 30, TFT_WHITE);
    tft.drawRect(160, 180, 32, 30, TFT_WHITE);
    tft.drawRect(192, 180, 32, 30, TFT_WHITE);
    tft.drawRect(224, 180, 32, 30, TFT_WHITE);
    tft.drawRect(256, 180, 64, 30, TFT_WHITE);

    tft.drawRect(0, 210, 40, 30, TFT_WHITE);
    tft.drawRect(40, 210, 40, 30, TFT_WHITE);
    tft.drawRect(80, 210, 120, 30, TFT_WHITE);
    tft.drawRect(200, 210, 40, 30, TFT_WHITE);
    tft.drawRect(240, 210, 40, 30, TFT_WHITE);
    tft.drawRect(280, 210, 40, 30, TFT_WHITE);

    if (shift){
      x1y1 = "Q";
      x2y1 = "W";
      x3y1 = "E";
      x4y1 = "R";
      x5y1 = "T";
      x6y1 = "Y";
      x7y1 = "U";
      x8y1 = "I";
      x9y1 = "O";
      x10y1 = "P";

      x1y2 = "A";
      x2y2 = "S";
      x3y2 = "D";
      x4y2 = "F";
      x5y2 = "G";
      x6y2 = "H";
      x7y2 = "J";
      x8y2 = "K";
      x9y2 = "L";
      x10y2 = "";

      x1y3 = "SHFT";
      x2y3 = "Z";
      x3y3 = "X";
      x4y3 = "C";
      x5y3 = "V";
      x6y3 = "B";
      x7y3 = "N";
      x8y3 = "M";
      x9y3 = "DEL";

      x1y4 = "?123";
      x2y4 = ",";
      x3y4 = "SPACE";
      x4y4 = ".";
      x5y4 = "BACK";
      x6y4 = "NEXT";
    }
    else if (number){
      x1y1 = "1";
      x2y1 = "2";
      x3y1 = "3";
      x4y1 = "4";
      x5y1 = "5";
      x6y1 = "6";
      x7y1 = "7";
      x8y1 = "8";
      x9y1 = "9";
      x10y1 = "0";

      x1y2 = "@";
      x2y2 = "#";
      x3y2 = "$";
      x4y2 = "_";
      x5y2 = "&";
      x6y2 = "-";
      x7y2 = "+";
      x8y2 = "(";
      x9y2 = ")";
      x10y2 = "/";

      x1y3 = "";
      x2y3 = "*";
      x3y3 = "\"";
      x4y3 = "'";
      x5y3 = ":";
      x6y3 = ";";
      x7y3 = "!";
      x8y3 = "?";
      x9y3 = "DEL";

      x1y4 = "?123";
      x2y4 = ",";
      x3y4 = "SPACE";
      x4y4 = ".";
      x5y4 = "BACK";
      x6y4 = "NEXT";
    }
    else{
      x1y1 = "q";
      x2y1 = "w";
      x3y1 = "e";
      x4y1 = "r";
      x5y1 = "t";
      x6y1 = "y";
      x7y1 = "u";
      x8y1 = "i";
      x9y1 = "o";
      x10y1 = "p";

      x1y2 = "a";
      x2y2 = "s";
      x3y2 = "d";
      x4y2 = "f";
      x5y2 = "g";
      x6y2 = "h";
      x7y2 = "j";
      x8y2 = "k";
      x9y2 = "l";
      x10y2 = "";

      x1y3 = "SHFT";
      x2y3 = "z";
      x3y3 = "x";
      x4y3 = "c";
      x5y3 = "v";
      x6y3 = "b";
      x7y3 = "n";
      x8y3 = "m";
      x9y3 = "DEL";

      x1y4 = "?123";
      x2y4 = ",";
      x3y4 = "SPACE";
      x4y4 = ".";
      x5y4 = "BACK";
      x6y4 = "NEXT";

    }

    tft.drawString(x1y1, 13,125,2);
    tft.drawString(x2y1, 45,125,2);
    tft.drawString(x3y1, 77,125,2);
    tft.drawString(x4y1, 109,125,2);
    tft.drawString(x5y1, 141,125,2);
    tft.drawString(x6y1, 173,125,2);
    tft.drawString(x7y1, 205,125,2);
    tft.drawString(x8y1, 237,125,2);
    tft.drawString(x9y1, 269,125,2);
    tft.drawString(x10y1,301,125,2);

    tft.drawString(x1y2, 13,155,2);
    tft.drawString(x2y2, 45,155,2);
    tft.drawString(x3y2, 77,155,2);
    tft.drawString(x4y2, 109,155,2);
    tft.drawString(x5y2, 141,155,2);
    tft.drawString(x6y2, 173,155,2);
    tft.drawString(x7y2, 205,155,2);
    tft.drawString(x8y2, 237,155,2);
    tft.drawString(x9y2, 269,155,2);
    tft.drawString(x10y2,301,155,2);

    tft.drawString(x1y3, 4,192,1);
    tft.drawString(x2y3, 45,185,2);
    tft.drawString(x3y3, 77,185,2);
    tft.drawString(x4y3, 109,185,2);
    tft.drawString(x5y3, 141,185,2);
    tft.drawString(x6y3, 173,185,2);
    tft.drawString(x7y3, 205,185,2);
    tft.drawString(x8y3, 237,185,2);
    tft.drawString(x9y3, 269,185,2);
    tft.drawString(x10y3,310,185,2);

    tft.drawString(x1y4, 5,217,2);
    tft.drawString(x2y4, 60,217,2);
    tft.drawString(x3y4, 117,217,2);
    tft.drawString(x4y4, 220,217,2);
    tft.drawString(x5y4, 245,217,2);
    tft.drawString(x6y4, 285,217,2);



  //shift
    if ((A >= 0) && (A < 20) && (B >= 185) && (B < 220) && (number == 0)){
      pressx1y3 = true;
    }
    else{
      if (pressx1y3){
        shift = !shift;
        pressx1y3 = false;
      }
    }
  //number
    if ((A >= 0) && (A < 30) && (B >= 220) && (B <= 240)){
      pressx1y4 = true;
    }
    else{
      if (pressx1y4){
        number = !number;
        shift = false;
        pressx1y4 = false;
      }
    }
  //delete
    if ((A >= 265) && (A <= 320) && (B >= 190) && (B < 220)){
      pressx9y3 = true;
    }
    else{
      if (pressx9y3){
        text = text.substring(0, text.length()-1);
        pressx9y3 = false;
      }
    }
  //space
    if ((A >= 75) && (A < 200) && (B >= 220) && (B <= 240)){
      pressx3y4 = true;
    }
    else{
      if (pressx3y4){
        text += " ";
        pressx3y4 = false;
      }
    }
  //back
    if ((A >= 245) && (A < 290) && (B >= 220) && (B <= 240)){
      pressx5y4 = true;
    }
    else{
      if (pressx5y4){
        if (state == 1){
          state = -1;
          text = "";
        }
        else if (state == 2){
          state = 1;
          if (wifi.length() != 0){
            text = wifi;
          }
          else {
            text = "";
          }
        }
        pressx5y4 = false;
      }
    }
  //next
    if ((A >= 290) && (A <= 320) && (B >= 220) && (B <= 240)){
      pressx6y4 = true;
    }
    else{
      if (pressx6y4){
        if (state == 1){
          state = 2;
          if (pass.length() != 0){
            text = pass;
          }
          else{
            text = "";
          }
        }
        else if (state == 2){
          state = 6;
          text = "";
        }
        }
        pressx6y4 = false;
      }
    
  //alphabet
    if ((A >= 0) && (A < 20) && (B >= 120) && (B < 150)){
      pressx1y1 = true;
    }
    else{
      if (pressx1y1){
        text += x1y1;
        pressx1y1 = false;
      }
    }

    if ((A >= 20) && (A < 55) && (B >= 120) && (B < 150)){
      pressx2y1 = true;
    }
    else{
      if (pressx2y1){
        text += x2y1;
        pressx2y1 = false;
      }
    }

    if ((A >= 55) && (A < 90) && (B >= 120) && (B < 150)){
      pressx3y1 = true;
    }
    else{
      if (pressx3y1){
        text += x3y1;
        pressx3y1 = false;
      }
    }
    
    if ((A >= 90) && (A < 125) && (B >= 120) && (B < 150)){
      pressx4y1 = true;
    }
    else{
      if (pressx4y1){
        text += x4y1;
        pressx4y1 = false;
      }
    }

    if ((A >= 125) && (A < 160) && (B >= 120) && (B < 150)){
      pressx5y1 = true;
    }
    else{
      if (pressx5y1){
        text += x5y1;
        pressx5y1 = false;
      }
    }
    
    if ((A >= 160) && (A < 195) && (B >= 120) && (B < 150)){
      pressx6y1 = true;
    }
    else{
      if (pressx6y1){
        text += x6y1;
        pressx6y1 = false;
      }
    }

    if ((A >= 195) && (A < 230) && (B >= 120) && (B < 150)){
      pressx7y1 = true;
    }
    else{
      if (pressx7y1){
        text += x7y1;
        pressx7y1 = false;
      }
    }

    if ((A >= 230) && (A < 265) && (B >= 120) && (B < 150)){
      pressx8y1 = true;
    }
    else{
      if (pressx8y1){
        text += x8y1;
        pressx8y1 = false;
      }
    }

    if ((A >= 265) && (A < 297) && (B >= 120) && (B < 150)){
      pressx9y1 = true;
    }
    else{
      if (pressx9y1){
        text += x9y1;
        pressx9y1 = false;
      }
    }

    if ((A >= 297) && (A <= 320) && (B >= 120) && (B < 150)){
      pressx10y1 = true;
    }
    else{
      if (pressx10y1){
        text += x10y1;
        pressx10y1 = false;
      }
    }

    if ((A >= 0) && (A < 20) && (B >= 150) && (B < 185)){
      pressx1y2 = true;
    }
    else{
      if (pressx1y2){
        text += x1y2;
        pressx1y2 = false;
      }
    }

    if ((A >= 20) && (A < 55) && (B >= 150) && (B < 185)){
      pressx2y2 = true;
    }
    else{
      if (pressx2y2){
        text += x2y2;
        pressx2y2 = false;
      }
    }

    if ((A >= 55) && (A < 90) && (B >= 150) && (B < 185)){
      pressx3y2 = true;
    }
    else{
      if (pressx3y2){
        text += x3y2;
        pressx3y2 = false;
      }
    }
    
    if ((A >= 90) && (A < 125) && (B >= 150) && (B < 185)){
      pressx4y2 = true;
    }
    else{
      if (pressx4y2){
        text += x4y2;
        pressx4y2 = false;
      }
    }

    if ((A >= 125) && (A < 160) && (B >= 150) && (B < 185)){
      pressx5y2 = true;
    }
    else{
      if (pressx5y2){
        text += x5y2;
        pressx5y2 = false;
      }
    }
    
    if ((A >= 160) && (A < 195) && (B >= 150) && (B < 185)){
      pressx6y2 = true;
    }
    else{
      if (pressx6y2){
        text += x6y2;
        pressx6y2 = false;
      }
    }

    if ((A >= 195) && (A < 230) && (B >= 150) && (B < 185)){
      pressx7y2 = true;
    }
    else{
      if (pressx7y2){
        text += x7y2;
        pressx7y2 = false;
      }
    }

    if ((A >= 230) && (A < 265) && (B >= 150) && (B < 185)){
      pressx8y2 = true;
    }
    else{
      if (pressx8y2){
        text += x8y2;
        pressx8y2 = false;
      }
    }

    if ((A >= 265) && (A < 297) && (B >= 150) && (B < 185)){
      pressx9y2 = true;
    }
    else{
      if (pressx9y2){
        text += x9y2;
        pressx9y2 = false;
      }
    }

    if ((A >= 297) && (A <= 320) && (B >= 150) && (B < 185)){
      pressx10y2 = true;
    }
    else{
      if (pressx10y2){
        text += x10y2;
        pressx10y2 = false;
      }
    }

    if ((A >= 20) && (A < 55) && (B >= 185) && (B < 220)){
      pressx2y3 = true;
    }
    else{
      if (pressx2y3){
        text += x2y3;
        pressx2y3 = false;
      }
    }

    if ((A >= 55) && (A < 90) && (B >= 185) && (B < 220)){
      pressx3y3 = true;
    }
    else{
      if (pressx3y3){
        text += x3y3;
        pressx3y3 = false;
      }
    }
    
    if ((A >= 90) && (A < 125) && (B >= 185) && (B < 220)){
      pressx4y3 = true;
    }
    else{
      if (pressx4y3){
        text += x4y3;
        pressx4y3 = false;
      }
    }

    if ((A >= 125) && (A < 160) && (B >= 185) && (B < 220)){
      pressx5y3 = true;
    }
    else{
      if (pressx5y3){
        text += x5y3;
        pressx5y3 = false;
      }
    }
    
    if ((A >= 160) && (A < 195) && (B >= 185) && (B < 220)){
      pressx6y3 = true;
    }
    else{
      if (pressx6y3){
        text += x6y3;
        pressx6y3 = false;
      }
    }

    if ((A >= 195) && (A < 230) && (B >= 185) && (B < 220)){
      pressx7y3 = true;
    }
    else{
      if (pressx7y3){
        text += x7y3;
        pressx7y3 = false;
      }
    }

    if ((A >= 230) && (A < 265) && (B >= 185) && (B < 220)){
      pressx8y3 = true;
    }
    else{
      if (pressx8y3){
        text += x8y3;
        pressx8y3 = false;
      }
    }
    
    if ((A >= 30) && (A < 75) && (B >= 220) && (B <= 240)){
      pressx2y4 = true;
    }
    else{
      if (pressx2y4){
        text += x2y4;
        pressx2y4 = false;
      }
    }

    if ((A >= 200) && (A < 245) && (B >= 220) && (B <= 240)){
      pressx4y4 = true;
    }
    else{
      if (pressx4y4){
        text += x4y4;
        pressx4y4 = false;
      }
    }
    // wifi name
    if (state == 1){
      wifi = text;
      tft.drawString("WiFi name = ", 32, 40,2 );
      tft.drawString(wifi,110, 40,2 );
    }
    else if (state == 2){
      pass = text;
      tft.drawString("WiFi password = ", 32, 40,2 );
      tft.drawString(pass,140, 40,2 );
    }
  }
  if (state == 6){
    tft.drawString("CONFIRM", 130, 40,2 );
    tft.drawString("WiFi name: ", 32, 60,2 );
    tft.drawString(wifi, 105, 60,2 );
    tft.drawString("Password: ", 32, 80,2 );
    tft.drawString(pass, 105, 80,2 );


    tft.drawRect(35, 170, 90, 50, TFT_WHITE);
    tft.drawString("BACK", 65, 185,2 );
    tft.drawRect(195, 170, 90, 50, TFT_WHITE);
    tft.drawString("NEXT", 225, 185,2 );
    if ((A >= 25) && (A < 120) && (B >= 175) && (B < 230)){
      pressonline = true;
    }
    else{
      if (pressonline){
        state = 2;
        if (pass.length() != 0){
          text = pass;
        }
        else {
          text = "";
        }
        pressonline = false;
      }
    }
    if ((A >= 200) && (A < 295) && (B >= 175) && (B < 230)){
      pressoffline = true;
    }
    else {
      if (pressoffline){
        Serial.println(wifi);
        ssid = wifi;
        Serial.println(pass);
        password = pass;
        state = 7;
        text = "";
        pressoffline = false;
        connectwifi = true;
      }
    }
    }
  if (state == 7){
    tft.drawString("CONNECTING TO WIFI", 90, 40,2 );
    tft.drawRect(35, 170, 90, 50, TFT_WHITE);
    tft.drawString("RETRY", 60, 185,2 );
    if ((A >= 25) && (A < 120) && (B >= 175) && (B < 230)){
      pressonline = true;
    }
    else{
      if (pressonline){
        state = 1;
        if (wifi.length() != 0){
          text = wifi;
        }
        else{
          text = "";
        }
        pressonline = false;
      }
    }
    if (connectwifi){
      WiFi.mode(WIFI_STA);
      WiFi.begin(ssid, password);
    }
    connectwifi = false;
    Serial.println(WiFi.status());
    if (WiFi.status() != WL_CONNECTED){
        Serial.print(".");
        if (numcount < 3){
          tft.drawString(".", 130+(numcount*20), 70,2 );
          numcount += 1;
        }
        else{
          numcount = 0;
        }
        delay(80);
    }
    else{
      Serial.println("CONNECT TO WIFI SUCCESSFUL");
      tft.drawString("CONNECTING SUCCESS", 87, 70,2 );
      configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
      client.setServer(mqtt_broker, mqtt_port);
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      delay(500);
      state = 8; //////////////////////////skip gps to state 9
      gpscount = 0;
    }
  }
  
  if (state == 8){
    tft.drawString("GETTING LOCATION", 100, 40,2 );
    tft.drawString("PLEASE WAIT A MOMENT", 80, 70,2 );
    tft.drawString("#Sat ", 130, 120,2);
    if(gpscount >= 5){
    while (ss.available() > 0)
      if (gps.encode(ss.read())){
        if (gps.location.isValid()){
          latitude = gps.location.lat();
          longitude = gps.location.lng();
          Serial.println(latitude,6);
          Serial.println(longitude,6);
          state = 9;
        }
        else{
          delay(1000);
          tft.drawString(String(gps.satellites.value()), 170,120,2);
          Serial.println(F("INVALID"));
        }
      }
    }
      gpscount += 1;
  }

  if (state == 9){
    tft.drawString("TESTING", 130, 40,2 );
    tft.drawString("1. INSERT SOLUTION", 32, 60,2 );
    tft.drawRect(110, 130, 90, 50, TFT_WHITE);
    tft.drawString("NEXT", 140, 145,2 );
    if ((A >= 105) && (A < 205) && (B >= 130) && (B < 185)){
      presstest1 = true;
    }
    else{
      if (presstest1){
        delay(250);
        Serial.println("Start test1");
        concentesttemt();
        intense1 = conoutput;
        Serial.println("got 1st concentration");  //1st concentration
        delay(500);
        state = 11;
        presstest1 = false;
      }
    }
  }
  if (state == 11){
    tft.drawString("TESTING", 130, 40,2 );
    tft.drawString("2. MIX THEN INSERT THE SOLUTION", 32, 60,2 );
    tft.drawRect(110, 130, 90, 50, TFT_WHITE);
    tft.drawString("NEXT", 140, 145,2 );
    if ((A >= 105) && (A < 205) && (B >= 130) && (B < 185)){
      presstest1 = true;
    }
    else{
      if (presstest1){
        delay(250);
        Serial.println("Start test2");
        concentesttemt();
        intense2 = conoutput;
        Serial.println("got 2nd concentration");  //2nd concentration
        delay(500);
        state = 12;
        presstest1 = false;
      }
    }
  }
  if (state == 12){
    tft.drawString("RESULT", 130, 40,2 );
    cubicsolve();
    concenstr = String(cube_ans);
    tft.drawString("Iron concentration equals", 80, 70,2 );
    tft.drawString(concenstr, 130, 100,2 );
    tft.drawString("ppm", 210, 100,2 );
    tft.drawRect(110, 130, 90, 50, TFT_WHITE);
    tft.drawString("NEXT", 140, 145,2 );
    if ((A >= 105) && (A < 205) && (B >= 130) && (B < 185)){
      presstest1 = true;
    }
    else{
      if (presstest1){
        if(online){
          state = 13;
        }
        else{
          state = 14;
        }
        presstest1 = false;
      }
    }
    delay(20);
  }
  if (state == 13){
    printLocalTime();
    Serial.println("---------------");
    Serial.println(currentdate);
    Serial.println("---------------");
    strlati = String(latitude,6);
    strlongi = String(longitude,6);
    Serial.println(strlati);
    Serial.println(strlongi);
    sendmes = currentdate+","+concenstr+","+"ppm"+","+ strlati+","+strlongi;
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    if (!client.connect(clientId.c_str())) {
      reconnect(); 
    }
    snprintf (mqtt_payload, Max_string, sendmes.c_str());
    client.publish(topic,mqtt_payload);
    Serial.println(mqtt_payload);
    delay(1000);
    state = 14;
  }

  if (state == 14){
    tft.drawString("END", 150, 40,2 );
    tft.drawRect(195, 170, 90, 50, TFT_WHITE);
    tft.drawString("RETRY", 225, 185,2 );
    if ((A >= 200) && (A < 295) && (B >= 175) && (B < 230)){
      presstest1 = true;
    }
    else{
      if (presstest1){
        state = 9;
        presstest1 = false;
      }
    }
    tft.drawRect(35, 170, 90, 50, TFT_WHITE);
    tft.drawString("MENU", 60, 185,2 );
    if ((A >= 25) && (A < 120) && (B >= 175) && (B < 230)){
      pressonline = true;
    }
    else{
      if (pressonline){
        state = -1;
        pressonline = false;
      }
    }
  }
    A = -1;
    B = -1;
    delay(30);
}
