#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <EEPROM.h>
#include <Servo.h>
#include "SSD1306Wire.h"
#include <BH1750.h>
#include "Wire.h"
#include "WiFiUdp.h"
#include <NTPClient.h>
#include "credentials.h"

const char* ssid = _ssid;
const char* password = _password;
const String url = _url;
const char* host = _host;


SSD1306Wire oled(0x3C, D4,D3);
Servo myservo; 
int pos = 90;
int increment = -1;
int lightValue;
String line;
String messageMode;
char idSaved; 
bool wasRead;  
BH1750 lightMeter;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
unsigned long epochTime;

unsigned long getInLoveSince(){
  timeClient.update();
  unsigned long inLoveSince = 1630800000;
  unsigned long now = timeClient.getEpochTime();
  return ((now-inLoveSince)/60/60/24)+1;
}

void initOled(){
  oled.init();
  oled.flipScreenVertically();
  oled.setColor(WHITE);
  oled.setTextAlignment(TEXT_ALIGN_LEFT);
  oled.setFont(ArialMT_Plain_10);
}

void mainScreen(){
  initOled();
  oled.clear();
  if(WiFi.status()==WL_CONNECTED){
    oled.drawString(25,4,String(getInLoveSince()) + " dni spolu <3");
    int32_t rssi = WiFi.RSSI();
    if(rssi>-50){
      oled.drawString(37,50,"Wi-Fi - 100%");
    }else if (rssi>=-60){
      oled.drawString(37,50,"Wi-Fi - 80%");
    }else if (rssi>=-70){
      oled.drawString(37,50,"Wi-Fi - 50%");
    }else if (rssi>=-80){
      oled.drawString(37,50,"Wi-Fi - 30%");
    }else if (rssi>=-90){
      oled.drawString(37,50,"Wi-Fi - 10%");
    }
  }
  oled.drawString(26, 30, "<3 LOVEBOX <3");
  oled.display();
}

float getLightStat(){
  Wire.begin(D2,D1);
  lightMeter.begin();
  float lux = lightMeter.readLightLevel();
  return lux;

}

void drawMessage(const String& message) {
  initOled();
  oled.clear();

  // differentiat between 't'ext and image message
  if(messageMode[0] == 't'){
    oled.drawStringMaxWidth(0, 0, 128, message);    
  } 
  else {
    for(int i = 0; i <= message.length(); i++){
      int x = i % 129;
      int y = i / 129;
    
      if(message[i] == '1'){
        oled.setPixel(x, y);
      }
    } 
  }    
  oled.display();
}

void wifiConnect() {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(ssid, password);

    // connecting to WiFi
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.println("Connecting");
    }
    Serial.println("Connected");
    mainScreen();
  }
}

void getGistMessage() {
  const int httpsPort = 443;
 
  const char fingerprint[] = "BD DE 0F 6B C8 5F CD AB 7F 46 05 DF 99 AB 78 B8 C3 C2 73 E9";
  Serial.println("Getting message");
  WiFiClientSecure client;
  client.setFingerprint(fingerprint);
  if (!client.connect(host, httpsPort)) {
    Serial.println("Connection failed");
    return;
  }
  
  // Make HTTP GET request
  client.print(String("GET " + String(url) + " HTTP/1.1\r\n") +
               "Host: " + host + "\r\n" +
               "Connection: close\r\n\r\n");

   while (client.connected()) {
    String temp = client.readStringUntil('\n');
    if (temp == "\r") {
      break;
    }
  }
  String id = client.readStringUntil('\n'); 
  if(id[0] != idSaved){ // new message
    messageMode = client.readStringUntil('\n');
    if (messageMode[0] == 't'){
      line = client.readStringUntil(0);
    } else {
      // binary image is corrupted if readStringUntil() takes too long
      // fix: read string line by line
      line = "";
      for (int i = 0; i < 64; i++)     
      {
        line += client.readStringUntil('\n');
        line += "\n";
      }
      if (line.length() != 8256)
      {
        getGistMessage();
      }
    }
    wasRead = 0;
    idSaved = id[0];
    EEPROM.write(142, idSaved);
    EEPROM.write(144, wasRead);
    EEPROM.commit(); 
    drawMessage(line);
    Serial.println(line);
  }
}

void spinServo(){
    for (pos = 0; pos <= 180; pos += 1) { // goes from 0 degrees to 180 degrees
      // in steps of 1 degree
        myservo.write(pos);              // tell servo to go to position in variable 'pos'

        delay(15);                       // waits 15 ms for the servo to reach the position
      }
      for (pos = 180; pos >= 0; pos -= 1) { // goes from 180 degrees to 0 degrees
        myservo.write(pos);              // tell servo to go to position in variable 'pos'

        delay(15);                       // waits 15 ms for the servo to reach the position
      }
}


void setup() {
  myservo.attach(D5);       // Servo on D0
  myservo.write(95);
  Serial.begin(9600);
  mainScreen();
     
  
  
  wifiConnect();
  
  EEPROM.begin(512);
  idSaved = EEPROM.get(142, idSaved);
  wasRead = EEPROM.get(144, wasRead);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    wifiConnect();
  }
  
  if(wasRead){
    getGistMessage();   
  }
 
  while(!wasRead){   
    yield();
    spinServo();    // turn heart
    lightValue = getLightStat();     // read light value
    if(lightValue > 0) { 
      wasRead = 1;
      EEPROM.write(144, wasRead);
      EEPROM.commit();
      myservo.write(95);
      delay(3500);
      mainScreen();
    }
  }
  
  delay(5000); // wait a five seconds
}
