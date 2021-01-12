#include <SPI.h>
#include <Adafruit_ST7735.h>
#include <Adafruit_GFX.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <Arduino_JSON.h>
#include <ESP8266WebServer.h>
#include <espnow.h>

#include "index.h"

//defining ports
#define buzzer    D7
#define button    D6
#define TFT_LEDA  D0
#define TFT_CS    D1
#define TFT_RST   0
#define TFT_DC    D2

#define TFT_SCLK 14
#define TFT_MOSI 2

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST); //define lcd

ESP8266WebServer server(255);  //init web server

//wifi and access point credentials
const char* ssid = "Livebox-AF2D";
const char* password = "EA18FFE6EC4108E4A951359213";
const char* ap_ssid = "NodeMCUv3";

//weather map api credentials
String openWeatherMapApiKey = "5ea1a7aceff07eb9ee88091ca077cd84";
String city = "Radlin";
String countryCode = "PL";

//slaves mac addresses
uint8_t broadcastAddressFan[] = {0xDC, 0x4F, 0x22, 0x78, 0x38, 0xAA};
uint8_t broadcastAddressLight[] = {0xDC, 0x4F, 0x22, 0x78, 0x38, 0xA9};

//timers init
unsigned long lastTime = 0;
const unsigned long timerDelay = 3600000;
unsigned long lastTimeSend = 0;
const unsigned long timerDelaySend = 3000;
unsigned long lastTimeWeather = 0;
const unsigned long timerDelayWeather = 8000;

bool showWeather1 = true;
bool showWeather2 = false;
bool weather1 = true;
bool weather2 = false;

bool clicked = true;

String temperature, humidity, windSpeed, pressure;

String jsonBuffer;

//HTML page init
String htmlContent = MAIN_page;

typedef struct structMessageFan {
  bool controlAuto;
  bool isOn;
  int temperatureLevel;
} structMessageFan;

typedef struct structMessageLight {
  bool controlAuto;
  bool isOn;
  int lightLevel;
} structMessageLight;

structMessageFan fanData;
structMessageLight lightData;

void setup() {

  fanData.controlAuto = true;
  fanData.isOn = true;
  lightData.controlAuto = true;
  lightData.isOn = true;

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(TFT_LEDA, OUTPUT);
  pinMode(buzzer, OUTPUT);
  pinMode(button, INPUT_PULLUP);

  //configure static ip
  IPAddress ip = ipaddr_addr("192.168.1.200");
  IPAddress gateway = ipaddr_addr("192.168.1.1");
  IPAddress subnet = ipaddr_addr("255.255.255.000");
  IPAddress dns1 = ipaddr_addr( "8.8.8.8" );
  IPAddress dns2 = ipaddr_addr( "8.8.4.4" );

  // initialize a ST7735S lcd screen
  tft.initR(INITR_BLACKTAB);
  Serial.begin(115200);
  //make NodeMCU a soft access-point
  WiFi.softAP(ap_ssid);
  WiFi.config(ip, gateway, subnet, dns1, dns2);
  //connecting to wifi
  WiFi.begin(ssid, password);
  Serial.println("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  //sound signal after booting
  tone(buzzer, 1000);
  delay(500);
  noTone(buzzer);
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());

  Serial.print("Connected as acces point to: ");
  Serial.print(ap_ssid);
  Serial.print(" : ");
  Serial.print(WiFi.softAPIP());
  //if successfully connected to wifi network - light screen and read weather
  if (WiFi.status() == WL_CONNECTED) {
    tft.fillScreen(ST7735_BLACK);  // fill screen with black color
    digitalWrite(TFT_LEDA, HIGH);
    readWeather();
  }

  //init espnow
  if (esp_now_init() != 0) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  //initialize espnow roles
  esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
  esp_now_register_send_cb(OnDataSent);
  esp_now_add_peer(broadcastAddressFan, ESP_NOW_ROLE_SLAVE, 1, NULL, 0);
  esp_now_add_peer(broadcastAddressLight, ESP_NOW_ROLE_SLAVE, 1, NULL, 0);

  //Handling requests from HTML
  server.on("/", handleRoot);
  server.on("/fanOn", handleFanOn);
  server.on("/fanOff", handleFanOff);
  server.on("/lightOn", handleLightOn);
  server.on("/lightOff", handleLightOff);
  server.on("/changeControlTypeFan", handleChangeControlTypeFan);
  server.on("/changeControlTypeLight", handleChangeControlTypeLight);
  server.on("/lightSlider", handleLightSlider);
  server.on("/temperatureSlider", handleTemperatureSlider);
  //Start server
  server.begin();
}

void loop() {
  //timer for changing weather tabs
  if ((millis() - lastTimeWeather) > timerDelayWeather) {
    if (weather1) {
      showWeather1 = true;
    }
    else if (weather2) {
      showWeather2 = true;
    }
    lastTimeWeather = millis();
  }

  writeWeather();

  //timer for sending packets to slaves
  if ((millis() - lastTimeSend) > timerDelaySend) {
    esp_now_send(broadcastAddressFan, (uint8_t *) &fanData, sizeof(fanData));
    esp_now_send(broadcastAddressLight, (uint8_t *) &lightData, sizeof(lightData));
    lastTimeSend = millis();
  }

  //button for turning on/off a lcd
  if (digitalRead(button) == LOW) {
    clicked = !clicked;
  }

  if (clicked) {
    digitalWrite(TFT_LEDA, HIGH);
  }
  else {
    digitalWrite(TFT_LEDA, LOW);
  }

  server.handleClient();

  //timer for reading weather
  if ((millis() - lastTime) > timerDelay) {
    readWeather();
    lastTime = millis();
  }
}

void OnDataSent(uint8_t *mac_addr, uint8_t sendStatus) {
  Serial.println("Last Packet Send Status: ");
  if (sendStatus == 0) {
    Serial.println("Delivery success");
  }
  else {
    Serial.println("Delivery fail");
  }
  Serial.println(lightData.lightLevel);
  Serial.println(lightData.controlAuto);
  Serial.println(lightData.isOn);
}

//Writes location of weather station
void writeLocation() {
  tft.setTextColor(ST7735_WHITE, ST7735_BLACK);
  tft.setTextSize(1);
  tft.setCursor(30, 16);
  tft.print("Location:");
  tft.setCursor(30, 33);
  tft.print(city + ", " + countryCode);
}

//Writes lines that separates information on a lcd
void writeLines() {
  tft.fillScreen(ST7735_BLACK);
  tft.drawFastHLine(0, 50,  tft.width(), ST7735_BLUE);
  tft.drawFastHLine(0, 102,  tft.width(), ST7735_BLUE);
}

//Writes page with humidity and temperature
void writeHumTemp(String temperature, String humidity) {
  if (showWeather1) {
    showWeather1 = false;

    writeLines();
    writeLocation();

    tft.setTextSize(1);
    tft.setTextColor(ST7735_GREEN, ST7735_BLACK);
    tft.setCursor(25, 61);
    tft.print("TEMPERATURE =");
    tft.setTextColor(ST7735_YELLOW, ST7735_BLACK);
    tft.setCursor(34, 113);
    tft.print("HUMIDITY =");
    tft.setTextSize(2);

    // print temperature (in °C)
    tft.setTextColor(ST7735_RED, ST7735_BLACK);
    tft.setCursor(20, 78);
    tft.print(temperature);
    tft.drawCircle(83, 80, 2, ST7735_RED);  // print degree symbol ( ° )
    tft.setCursor(89, 78);
    tft.print("C");

    // print humidity (in %)
    tft.setTextColor(ST7735_CYAN, ST7735_BLACK);
    tft.setCursor(20, 130);
    tft.print(humidity);
    tft.setCursor(89, 130);
    tft.print("%");
    weather1 = false;
    weather2 = true;
  }
}

//Writes page with wind speed and pressure
void writeSpeedPress(String windSpeed, String pressure) {
  if (showWeather2) {
    showWeather2 = false;

    writeLines();
    writeLocation();

    tft.setTextSize(1);
    tft.setTextColor(ST7735_GREEN, ST7735_BLACK);
    tft.setCursor(25, 61);
    tft.print("WIND SPEED =");
    tft.setTextColor(ST7735_YELLOW, ST7735_BLACK);
    tft.setCursor(34, 113);
    tft.print("PRESSURE =");
    tft.setTextSize(2);

    // print wind speed  (in km/h)
    tft.setTextColor(ST7735_RED, ST7735_BLACK);
    tft.setCursor(20, 78);
    tft.print(windSpeed);
    tft.setCursor(89, 78);
    tft.print("m/s");

    // print pressure (in hPa)
    tft.setTextColor(ST7735_CYAN, ST7735_BLACK);
    tft.setCursor(20, 130);
    tft.print(pressure);
    tft.setCursor(89, 130);
    tft.print("hPa");
    weather1 = true;
    weather2 = false;
  }
}

//Reads weather from OpenWeatherMap and writes it to variables
void readWeather() {
  String serverPath = "http://api.openweathermap.org/data/2.5/weather?q=" + city + "," + countryCode + "&units=metric" + "&APPID=" + openWeatherMapApiKey;

  jsonBuffer = httpGETRequest(serverPath.c_str());
  Serial.println(jsonBuffer);
  JSONVar myObject = JSON.parse(jsonBuffer);

  if (JSON.typeof(myObject) == "undefined") {
    Serial.println("Parsing input failed!");
    return;
  }

  humidity = JSON.stringify(myObject["main"]["humidity"]);
  temperature = JSON.stringify(myObject["main"]["temp"]);
  windSpeed = JSON.stringify(myObject["wind"]["speed"]);
  pressure = JSON.stringify(myObject["main"]["pressure"]);
}

//Gets request from http
String httpGETRequest(const char* serverName) {
  HTTPClient http;
  //Given server name "http://..."
  http.begin(serverName);

  // Send HTTP POST request
  int httpResponseCode = http.GET();

  String payload = "{}";

  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    payload = http.getString();
  }
  else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  http.end();

  return payload;
}

//Writes weather
void writeWeather() {

  writeHumTemp(temperature, humidity);
  writeSpeedPress(windSpeed, pressure);
}

//Displays main html page
void handleRoot() {
  server.send(200, "text/html", htmlContent);
}

//Functions below are responsible for handling user input (buttons) on html website.

void handleFanOn() {
  fanData.isOn = true;
  tone(buzzer, 1000);
  delay(50);
  noTone(buzzer);
}

void handleFanOff() {
  fanData.isOn = false;
  tone(buzzer, 1000);
  delay(50);
  noTone(buzzer);
}

void handleChangeControlTypeFan() {
  String state = "OFF";
  String act_state = server.arg("state");
  if (act_state == "1")
  {
    state = "ON";
    fanData.controlAuto = false;
  }
  else
  {
    state = "OFF";
    fanData.controlAuto = true;
  }
  server.send(200, "text/plane", state);
}

void handleChangeControlTypeLight() {
  String state = "OFF";
  String act_state = server.arg("state");
  if (act_state == "1")
  {
    state = "ON";
    lightData.controlAuto = false;
  }
  else
  {
    state = "OFF";
    lightData.controlAuto = true;
  }
  server.send(200, "text/plane", state);
}

void handleLightOn() {
  lightData.isOn = true;
  tone(buzzer, 1000);
  delay(50);
  noTone(buzzer);
}

void handleLightOff() {
  lightData.isOn = false;
  tone(buzzer, 1000);
  delay(50);
  noTone(buzzer);
}

void handleLightSlider() {
  String actSliderValue = server.arg("level");
  lightData.lightLevel = actSliderValue.toInt();
}

void handleTemperatureSlider() {
  String actSliderValue = server.arg("level");
  fanData.temperatureLevel = actSliderValue.toInt();
}
