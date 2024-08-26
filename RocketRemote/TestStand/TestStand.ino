/*
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com/esp-now-esp8266-nodemcu-arduino-ide/
  
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files.
  
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
*/

#include <ESP8266WiFi.h>
#include <espnow.h>

// REPLACE WITH RECEIVER MAC Address
uint8_t broadcastAddress[] = { 0xf4, 0x12, 0xfa, 0xce, 0x29, 0xf4 };

// Structure example to send data
// Must match the receiver structure
typedef struct struct_message {
  float thrust;
  float pressure;
  bool armed;
  long timestamp;
} struct_message;
// Create a struct_message called messageData
struct_message messageData;

typedef struct struct_command {
  bool armed;
  bool fire;
  bool reset;
} struct_command;
struct_command receivedCommands;

double values[] = { 0.36, 0.36, 0.37, 0.36, 0.38, 0.39, 0.38, 0.40, 0.42, 0.40, 0.38, 0.35, 0.31, 0.29, 0.24, 0.22, 0.20, 0.20, 0.20, 0.20, 0.20, 0.21, 0.20, 0.18, 0.18, 0.15, 0.13, 0.11, 0.13, 0.15, 0.17, 0.17, 0.20, 0.20, 0.20, 0.19, 0.19, 0.18, 0.16, 0.15, 0.14, 0.13, 0.12, 0.12, 0.13, 0.13, 0.13, 0.13, 0.13, 0.12, 0.10, 0.11, 0.12, 0.11, 0.09, 0.08, 0.09, 0.08, 0.06, 0.03, 0.05, 0.08, 0.08, 0.08, 0.08, 0.06, 0.08, 0.10, 0.10, 0.10, 0.10, 0.67, 3.40, 12.64, 56.19, 171.38, 288.67, 372.69, 449.36, 522.60, 593.30, 662.61, 730.81, 799.09, 867.66, 934.93, 1001.10, 1066.15, 1128.06, 1182.53, 1201.98, 1201.98, 1149.32, 1094.23, 1071.96, 1056.45, 1044.34, 1034.67, 1025.99, 1018.03, 1005.22, 960.49, 895.54, 830.06, 764.95, 700.75, 637.40, 574.72, 512.52, 450.59, 389.11, 328.18, 267.25, 206.46, 146.04, 85.64, 30.43, 7.07, 4.97, 4.51, 4.21, 3.98, 3.83, 3.69, 3.58, 3.49, 3.44, 3.41, 3.35, 3.29, 3.23, 3.16, 3.10, 3.05, 3.01, 2.98, 2.90, 2.83, 2.83, 3.90, 3.84, 3.24, 1.51, -0.24, -1.99, -3.76, -5.50, -7.23, -8.95, -10.66, -12.37, -14.08, -15.80, -17.55, -19.30, -21.02, -23.84, -25.02, -25.05, -25.05, -25.02, -25.00, -25.00, -24.99, -24.98, -24.98, -24.98, -24.98, -24.98, -24.99, -25.00, -25.00, -24.96, -24.96, -24.98, -24.97, -24.98, -24.98, -25.02, -25.07, -25.09, -25.09, -25.12, -25.14, -25.15, -25.16, -25.17, -25.13, -25.09, -25.09, -25.10, -25.08, -25.09, -25.10, -25.10, -25.13, -25.13, -25.13, -25.15, -25.16, -25.16, -25.16, -25.16, -25.17, -25.16, -25.19, -25.21, -25.20, -25.21, -25.21, -25.17, -25.17, -25.16, -25.15, -25.12, -25.08, -25.07, -25.09, -25.09, -25.06, -25.03, -25.02, -25.02, -25.02, -25.00, -25.01, -25.02, -25.03, -25.05, -25.04, -25.02, -25.00, -25.00, -25.02, -25.02, -25.00, -24.99, -25.00, -25.00, -25.02, -25.00, -25.00, -25.02, -25.02, -25.01, -25.00, -24.98, -24.98, -24.95, -24.94, -24.94, -24.92, -24.92, -24.93, -24.93, -24.92, -24.91, -24.92, -24.95, -24.93, -24.92, -24.93, -24.96, -24.99, -25.00, -25.02, -25.04, -25.05, -25.07, -25.09, -25.09, -25.08, -25.09, -25.11, -25.14, -25.13, -25.11, -25.11, -25.09, -25.06 };
unsigned long lastTime = 0;
unsigned long timerDelay = 150;  // send readings timer
int i = 0;
long lastMillis = 0;

#define warn 14
#define relayMiddle 12
#define relayOuter 13

// Callback when data is sent
void OnDataSent(uint8_t *mac_addr, uint8_t sendStatus) {
  // Serial.print("Last Packet Send Status: ");
  if (sendStatus != 0) {
    digitalWrite(warn, HIGH);
  } else {
    digitalWrite(warn, LOW);
  }
}

// callback function that will be executed when data is received
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
  memcpy(&receivedCommands, incomingData, sizeof(receivedCommands));
  messageData.armed = receivedCommands.armed;
}

void setup() {
  // Init Serial Monitor
  Serial.begin(115200);
  
  pinMode(warn, OUTPUT);
  pinMode(relayMiddle, OUTPUT);
  pinMode(relayOuter, OUTPUT);

    digitalWrite(relayMiddle, LOW);
    digitalWrite(relayOuter, LOW);

  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  // Init ESP-NOW
  if (esp_now_init() != 0) {
    Serial.println("Error initializing ESP-NOW");
    digitalWrite(warn, HIGH);
    return;
  }

  // Once ESPNow is successfully Init, we will register for Send CB to
  // get the status of Trasnmitted packet
  esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
  esp_now_register_send_cb(OnDataSent);

  // Register peer
  esp_now_add_peer(broadcastAddress, ESP_NOW_ROLE_SLAVE, 1, NULL, 0);
  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));

  messageData.armed = true;
}


void loop() {

  if (i >= 279) {
    i = 0;
  }
  // Set values to send
  messageData.thrust = values[i];
  messageData.pressure = 0;
  messageData.armed;
  messageData.timestamp = millis();
  // Serial.println(i);
  Serial.println(values[i]);
  i++;

  if (receivedCommands.fire){
    digitalWrite(relayMiddle, HIGH);
    digitalWrite(relayOuter, LOW);
  } else {
    digitalWrite(relayMiddle, LOW);
    digitalWrite(relayOuter, LOW);
  }

  // Send message via ESP-NOW
  esp_now_send(broadcastAddress, (uint8_t *)&messageData, sizeof(messageData));
  delay(100);
}
