#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <Wire.h>
#include <BH1750.h>
#include <SPI.h>
#include <espnow.h>

#define RELAY 3
#define sda 0
#define scl 2

const char* host = "192.168.4.1";      // address of access point - NodeMCU V3

BH1750 lightMeter (0x5C);

int currentLux = 0;

// structure that we assign received information to
typedef struct structMessageLight {
  bool controlAuto;
  bool isOn;
  int lightLevel;
} structMessageLight;

structMessageLight lightData;

void setup() {
  lightData.controlAuto = true;
  lightData.isOn = true;

  //Initializing pins
  pinMode(RELAY, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(RELAY, LOW);

  WiFi.mode(WIFI_STA);                 // make this microcontroller a station
  WiFi.begin("NodeMCUv3");             // connects to an access point - NodeMCU V3

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  //Check if connected to WiF i- if so make x20 fast blinks with builtin led
  if (WiFi.status() == WL_CONNECTED) {
    for (int i = 0; i < 20; i++) {
      digitalWrite(LED_BUILTIN, HIGH);
      delay(50);
      digitalWrite(LED_BUILTIN, LOW);
      delay(50);
    }
    delay(500);
  }
  // Init ESP-NOW
  if (esp_now_init() != 0) {
    return;
  }

  //Registering this microcontroler as data receiver
  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
  esp_now_register_recv_cb(OnDataRecv);

  //Initializing light sensor and definig SDA and SCL I2C lines
  Wire.begin (sda, scl);
  lightMeter.begin();
}

void loop(){
  if(WiFi.status() != WL_CONNECTED){
    lightData.controlAuto = false;
    lightData.isOn = false;
  }
  currentLux = lightMeter.readLightLevel();  //reading sensor
}

/**
 * Receiving frame with data
 */
void OnDataRecv(uint8_t * mac, uint8_t *incomingData, uint8_t len) {
  memcpy(&lightData, incomingData, sizeof(lightData));
  if (lightData.controlAuto) {
    if ( currentLux < lightData.lightLevel) {
      digitalWrite(RELAY, HIGH);
    }
    else {
      digitalWrite(RELAY, LOW);
    }
  }
  else {
    if (lightData.isOn) {
      digitalWrite(RELAY, HIGH);
    }
    else {
      digitalWrite(RELAY, LOW);
    }
  }
}
