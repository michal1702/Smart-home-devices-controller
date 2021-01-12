#include <ESP8266WiFi.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFiClient.h>
#include <SPI.h>
#include <espnow.h>

#define RELAY 3
#define SENSOR 0

const char* host = "192.168.4.1";      // address of access point - NodeMCU V3

OneWire oneWire(SENSOR);
DallasTemperature sensors(&oneWire);

// structure that we assign received information to
typedef struct structMessageFan {     
  bool controlAuto;
  bool isOn;
  int temperatureLevel;
} structMessageFan;

structMessageFan fanData;

float temp = 0.0f;

void setup() {
  fanData.controlAuto = true;
  fanData.isOn = true;
  sensors.begin();                     // starting sensor reading
  
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
}

void loop() {
    //Read sensor values
    sensors.requestTemperatures();
    temp = sensors.getTempCByIndex(0);
}

/**
 * Receiving frame with data
 */
void OnDataRecv(uint8_t * mac, uint8_t *incomingData, uint8_t len) {
  memcpy(&fanData, incomingData, sizeof(fanData));
  if (fanData.controlAuto) {
    if ( temp > fanData.temperatureLevel) {
      digitalWrite(RELAY, HIGH);
    }
    else {
      digitalWrite(RELAY, LOW);
    }
  }
  else {
    if (fanData.isOn) {
      digitalWrite(RELAY, HIGH);
    }
    else {
      digitalWrite(RELAY, LOW);
    }
  }
}
