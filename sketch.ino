#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <HTTPClient.h>
#include "ThingSpeak.h"

// Define note frequencies (in Hz)
#define NOTE_C4 261
#define NOTE_D4 294
#define NOTE_E4 329
#define NOTE_F4 349
#define NOTE_G4 392
#define NOTE_A4 440
#define NOTE_B4 494
// And so on for other notes as needed
const int melody[] = {NOTE_C4, NOTE_E4, NOTE_G4, NOTE_G4, NOTE_A4, NOTE_G4, 0};
const int noteDurations[] = {4, 4, 4, 4, 4, 2, 4};
      
WiFiUDP ntpUDP;
bool deviceState = false; // Default state is OFF
// You can specify the time offset in seconds (-25200 for UTC-7, 25200 for UTC+7).
NTPClient timeClient(ntpUDP, 25200);
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
const int pir_motion_sensor=4;
const int photoresistor = 2;
const int led=13;
const int buzzer=5;
const int ledBlinkSeg=200;
int ledTimer;
const char* ssid = "Wokwi-GUEST";
const char* password = "";
TaskHandle_t Task1,Task2;
SemaphoreHandle_t batton;
int thiefDetected=false;

const char* mqttServer = "test.mosquitto.org";
int port = 1883;
const char* writeAPIKey = "ALB634SM724GJ5R0";
const int channelID = 2233575;
WiFiClient espClient;
PubSubClient client(espClient);
// Custom buzzer_sound function to generate tones
void buzzer_sound(int frequency, int duration) {
  int period = 1000000 / frequency; // Calculate the period of the wave in microseconds
  int halfPeriod = period / 2; // Calculate the half period
  long elapsed_time = 0;

  while (elapsed_time < duration * 1000) { // Convert duration to microseconds
    digitalWrite(buzzer, HIGH);
    delayMicroseconds(halfPeriod);
    digitalWrite(buzzer, LOW);
    delayMicroseconds(halfPeriod);
    elapsed_time += period;
  }
}
void notifyIFTTT() {
  HTTPClient http;
  String url = "https://maker.ifttt.com/trigger/button/with/key/dUQxW3Sp7EOZmRPxA9PjTW";
  http.begin(url);
  int httpResponseCode = http.GET();
  if (httpResponseCode > 0) {
    Serial.println("IFTTT notification sent successfully!");
  } else {
    Serial.println("Error sending IFTTT notification!");
  }
  http.end();
}

void displayText(String st){
  oled.clearDisplay();
  oled.setTextColor(WHITE);
  oled.setCursor(50,0);
  oled.setTextSize(1);
  oled.println(getTimeString());
  oled.setCursor(40,20);
  oled.setTextSize(2);
  oled.println(st);
  oled.setCursor(35,50);
  oled.setTextSize(1);
  oled.println("8 August 2023");
  oled.display();
}

bool gaurdMode(){
  if (analogRead(photoresistor)==0){
    if (digitalRead(pir_motion_sensor)==0)
      
      displayText("ON");
  }else{
    displayText("OFF");
    
  }
  return (analogRead(photoresistor)==0);
}
bool checkForMotion(){
  if (!gaurdMode()){
      return false;
  }
  if (digitalRead(pir_motion_sensor)==1){
    displayText("WARNING");
    return true;
  }
  return false;
}
void ledBlinking(){
  if (millis()-ledTimer>=ledBlinkSeg){
    int a=digitalRead(led);
    digitalWrite(led,1-a);
    ledTimer=millis();
  }
}
void turnOnAlarm(){
  ledBlinking();
}
void turnOffAlarm(){
  digitalWrite(led,0);
}
void playMelody(const int melody[], const int noteDurations[]) {
  for (int i = 0; i < sizeof(melody) / sizeof(int); i++) {
    int noteDuration = 1000 / noteDurations[i];
    buzzer_sound(melody[i], noteDuration);
    delay(noteDuration);
  }
}

void checkForThief(){
  if (checkForMotion()){
    if (!thiefDetected) { // Only publish if state has changed
      Serial.println("Thief detected!");
      client.publish("21127058/temp", "Thief detected!"); // Publishes "1" to indicate thief is detected
      thiefDetected = true;
      ThingSpeak.writeField(channelID,1,getTimeString(),writeAPIKey);
      // Play a song to alert about the thief
       // Play a song to alert about the thief
      //  turnOnAlarm();
      
      const int melody[] = {262, 330, 392, 523, 392, 330, 262}; // Frequencies of the notes (adjust as needed)
      const int noteDurations[] = {500, 500, 500, 500, 500, 500, 500}; // Durations of the notes in milliseconds
      for (int i = 0; i < sizeof(melody) / sizeof(int); i++) {
        buzzer_sound(melody[i], noteDurations[i]);
      }
      notifyIFTTT();
    }
  }else{
    if (thiefDetected) { // Only publish if state has changed
      Serial.println("Thief gone!");
      client.publish("21127058/temp", "Thief gone!"); // Publishes "0" to indicate thief is gone
      thiefDetected = false;
    }
  }
}
void Task1code( void * parameter) {
  for(;;) {
    if (thiefDetected){
      // client.publish("21127058/temp", "Thief detected!"); // Publishes "1" to indicate thief is detected
      turnOnAlarm();
    }else{
      turnOffAlarm();
      // client.publish("21127058/temp", "Thief gone!"); // Publishes "0" to indicate thief is gone
    }
  }
}
void setup() {
  
  // put your setup code here, to run once:
  Serial.begin(9600);
  Serial.println("Connecting to WiFi");
  batton=xSemaphoreCreateBinary();
  xSemaphoreGive(batton);
  ThingSpeak.begin(espClient);
  wifiConnect();
  client.setServer(mqttServer, port);
  client.setCallback(callback);
  pinMode(photoresistor, INPUT);
  pinMode(pir_motion_sensor, INPUT);
  pinMode(led, OUTPUT);
  pinMode(buzzer,OUTPUT);
  if(!oled.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  oled.clearDisplay();
  oled.setTextColor(WHITE);
  oled.setCursor(50,0);
  oled.setTextSize(1);
  oled.println("9:00 am");
  oled.setCursor(35,50);
  oled.setTextSize(1);
  oled.println("8 August 2023");
  oled.display();
  xTaskCreatePinnedToCore(
      Task1code, /* Function to implement the task */
      "Task1", /* Name of the task */
      10000,  /* Stack size in words */
      NULL,  /* Task input parameter */
      0,  /* Priority of the task */
      &Task1,  /* Task handle. */
      0); /* Core where the task should run */
  delay(500);
  // xTaskCreatePinnedToCore(
  //     Task2code, /* Function to implement the task */
  //     "Task2", /* Name of the task */
  //     10000,  /* Stack size in words */
  //     NULL,  /* Task input parameter */
  //     1,  /* Priority of the task */
  //     &Task2,  /* Task handle. */
  //     1); /* Core where the task should run */
}
void mqttReconnect(){
  while(!client.connected()){
    Serial.println("Attempting MQTT connection...");
    if(client.connect("21127058")){
      Serial.println("Connected");
      client.subscribe("21127058/switch");
    }
    else{
      Serial.println("Try again in 5 seconds");
      delay(5000);
    }
  }
}
void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  if (message == "ON") {
    // Switch the device ON
    // For example, if this means setting a global variable to track the state
    deviceState = true;
  } 
  else if (message == "OFF") {
    // Switch the device OFF
    deviceState = false;
  }
}

void wifiConnect() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" Connected");
}
String getTimeString() {
  timeClient.update();
  return timeClient.getFormattedTime();
}
void loop() {
  if(!client.connected()){
    mqttReconnect();
  }
  
  client.loop();
  // Publish time
  String timeString = getTimeString();
  client.publish("21127058/time", timeString.c_str());
   if (deviceState) {
    client.publish("21127058/status", "ON");
    client.publish("21127058/temp", "No thief here"); 
    checkForThief();
  } 
  else{
    client.publish("21127058/status", "OFF");
    displayText("OFF");
    client.publish("21127058/temp", "Device is Off"); 
  }
  
}