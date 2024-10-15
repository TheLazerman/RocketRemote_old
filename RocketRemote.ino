#include "LV_Helper.h"
#include "ui.h"
#include <esp_now.h>
#include <WiFi.h>


// Structure example to receive data
// Must match the sender structure
typedef struct struct_message {
  float thrust;
  float pressure;
  bool armed;
  long timestamp;
} struct_message;
struct_message standData;

typedef struct struct_command {
  bool armed;
  bool fire;
  bool reset;
} struct_command;
struct_command testStandCommands;
esp_now_peer_info_t peerInfo;

uint8_t broadcastAddress[] = { 0xb4, 0xe6, 0x2d, 0x69, 0xd6, 0xa0 };

#define greenled 1
#define redled 2
#define arm 3
#define FIRE 10

long lastMillis = 0;
long lastMsg = 0;
bool armed = true;
long debounce = 0;
bool success;
float Max = 0;
float currentMax = 0;
float avg_thrust = 0;
float totalImpulse = 0;

std::list<float> thrust_curve = {};

static lv_chart_series_t * ser1;
static lv_coord_t ui_ThrustChart1_series_1_array[] = { };




// Callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  // Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
  if (status != 0){
    success = false;
    _ui_state_modify(ui_DisconnectAlert, _UI_MODIFY_STATE_REMOVE, LV_STATE_DISABLED);
  } else {
    success = true;
    _ui_state_modify(ui_DisconnectAlert, _UI_MODIFY_STATE_ADD, LV_STATE_DISABLED);
  }

}


void toggle_arm() {
  if (armed) {
    testStandCommands.armed = false;
  } else {
    testStandCommands.armed = true;
  }
  esp_err_t comResult = esp_now_send(broadcastAddress, (uint8_t *)&testStandCommands, sizeof(testStandCommands));
}


void fire() {
  if (armed){
    testStandCommands.fire = true;
    esp_err_t fireResult = esp_now_send(broadcastAddress, (uint8_t *)&testStandCommands, sizeof(testStandCommands));
  }
}


void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);
  Serial.println("Starting UI");
    
  lv_helper();
  ui_init();

  ser1 = lv_chart_add_series(ui_ThrustChart1, lv_color_hex(0x808080), LV_CHART_AXIS_PRIMARY_Y);

  analogWrite(38,255); // ?? What is this line doing? 

  pinMode(arm, INPUT_PULLUP);
  pinMode(FIRE, INPUT_PULLUP);
  pinMode(redled, OUTPUT);
  pinMode(greenled, OUTPUT);
     //Turn on display power
  pinMode(15, OUTPUT);
  digitalWrite(15, HIGH);


  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  // Register peer
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  // Add peer
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }
  
  // Once ESPNow is successfully Init, we will register for recv CB to
  // get recv packer info       
  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));
  esp_now_register_send_cb(OnDataSent);

}


// callback function that will be executed when data is received
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
  lastMsg = millis();
  memcpy(&standData, incomingData, sizeof(standData));
  armed = standData.armed;
  standData.thrust = GramsToNewtons(standData.thrust);
  currentMax = Max;
  if (standData.thrust > Max){
    Max = standData.thrust;
  }
  _ui_label_set_property(ui_MaxThrustValue, _UI_LABEL_PROPERTY_TEXT, String(Max).c_str());
  lv_chart_set_range(ui_ThrustChart1, LV_CHART_AXIS_PRIMARY_Y, 0, Max + 25 );
  if (currentMax != Max && standData.thrust <= 0){
    lv_chart_set_next_value(ui_ThrustChart1, ser1, standData.thrust);
  }
  thrust_curve.push_back(standData.thrust);
  avg_thrust = std::accumulate(thrust_curve.begin(), thrust_curve.end(), 0.0) / thrust_curve.size();
  _ui_label_set_property(ui_AvgThrustValue, _UI_LABEL_PROPERTY_TEXT, String(avg_thrust).c_str);
  totalImpulse = std::accumulate(thrust_curve.begin(), thrust_curve.end(), 0.0) * 0.1;
  _ui_label_set_property(ui_ImpulseValue, _UI_LABEL_PROPERTY_TEXT, string(totalImpulse).c_str);
}


float GramsToNewtons(float: grams){
  return (grams/1000)*9.81;
}


void loop() {
  if (standData.armed){
    digitalWrite(redled, HIGH);
    digitalWrite(greenled, LOW);
  } else {
    digitalWrite(redled, LOW);
    digitalWrite(greenled, HIGH);
  }
  if (millis() - debounce > 250 ){
    if (digitalRead(arm) == 0) {
      toggle_arm();
      debounce = millis();
    }
    if (digitalRead(FIRE) == 0) {
      fire();
      debounce = millis();
    }
  }

  lv_task_handler();
  delay(5);
}

