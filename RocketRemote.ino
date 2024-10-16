#include "LV_Helper.h"
#include "ui.h"
#include <esp_now.h>
#include <WiFi.h>
#include <vector>
#include <numeric>
#include <cmath>

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

#define safeLED 1
#define armedLED 2
#define armingButton 3
#define ignitionButton 10

long lastMillis = 0;
long lastMsg = 0;
long debounce = 0;

bool armed = false;
bool eventInProgress = false;
bool eventOccurred = false;
bool done = false;

double maxThrust = 0;
double lastThrust = 0;
double avgThrust = 0;
double totalImpulse = 0;
double rateOfChange = 0;

double eventStartThreshold = 0.03;
double eventStopThreshold = 0.04;
int windowSize = 3;

std::vector<double> thrustCurve;
static std::vector<double> filteredData;

static lv_chart_series_t * ser1;
static lv_coord_t ui_ThrustChart1_series_1_array[] = { };

// Callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  // Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
  if (status != 0){
    _ui_state_modify(ui_DisconnectAlert, _UI_MODIFY_STATE_REMOVE, LV_STATE_DISABLED);
  }
   _ui_state_modify(ui_DisconnectAlert, _UI_MODIFY_STATE_ADD, LV_STATE_DISABLED);
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

  pinMode(armingButton, INPUT_PULLUP);
  pinMode(ignitionButton, INPUT_PULLUP);
  pinMode(armedLED, OUTPUT);
  pinMode(safeLED, OUTPUT);
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


double GramsToNewtons(double grams){
  return (grams/1000)*9.81;
}


// Function to calculate the instantaneous rate of change
double calculateInstantaneousRateOfChange(const std::vector<double>& data, int currentIndex) {
  // Check if there's enough data to calculate the rate of change
  if (currentIndex < 1 || currentIndex > data.size()) {
    return 0.0; // Or handle the error appropriately
  }

  // Calculate the rate of change using the previous and current data points
  double rateOfChange = (data[currentIndex] - data[currentIndex - 1]); 
  return rateOfChange;
}

// Function to apply a moving average filter
std::vector<double> applyMovingAverageFilter(const std::vector<double>& data, int windowSize) {
  for (int i = 0; i < data.size(); ++i) {
    // Calculate the start and end indices for the averaging window
    int start = std::max(0, i - windowSize / 2);
    int end = std::min(static_cast<int>(data.size() - 1), i + windowSize / 2);

    double sum = 0.0;
    for (int j = start; j <= end; ++j) {
      sum += data[j];
    }

    filteredData.push_back(sum / (end - start + 1));
  }
  return filteredData;
}

// Function to detect event start and stop based on rate of change
bool detectEvent(double rateOfChange, double eventStartThreshold, double eventStopThreshold) {
  // Check if the rate of change exceeds the start threshold (event begins)
  if (std::abs(rateOfChange) > eventStartThreshold) {
    eventOccurred = true; // An event has infact, occurred
    return true; // Event started
  } 
  // Check if the rate of change falls below the stop threshold (event ends)
  else if (std::abs(rateOfChange) < eventStopThreshold) { 
    return false; // Event stopped 
  } 
  // If neither condition is met, maintain the previous state
  else {
    static bool eventInProgress = false; // Keep track of the event state
    return eventInProgress; 
  }
}


void center_chart_series(lv_obj_t * chart, lv_chart_series_t * ser) {
  // 1. Get the minimum and maximum x-values of the data series
  lv_coord_t min_x = 0; // Assuming x starts from 0
  lv_coord_t max_x = lv_chart_get_point_count(chart) - 1; // Last x-value

  // 2. Calculate the middle x-value
  lv_coord_t mid_x = (min_x + max_x) / 2;

  // 3. Calculate the shift needed to center the data
  lv_coord_t shift = lv_chart_get_point_count(chart) / 2 - mid_x;

  // 4. Shift the x-values of the data points
  for (uint16_t i = 0; i < lv_chart_get_point_count(chart); i++) {
    ser->x_points[i] += shift; 
  }

  // 5. Refresh the chart to reflect the changes
  lv_chart_refresh(chart);
}


// callback function that will be executed when data is received
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
  lastMsg = millis();
  memcpy(&standData, incomingData, sizeof(standData));
  armed = standData.armed;

  if (armed){
    digitalWrite(armedLED, HIGH);
    digitalWrite(safeLED, LOW);
  } else {
    digitalWrite(armedLED, LOW);
    digitalWrite(safeLED, HIGH);
  }
  if (eventOccurred && !eventInProgress){
    return;
  }
  standData.thrust = GramsToNewtons(standData.thrust);
  if (standData.thrust > maxThrust){
    maxThrust = standData.thrust;
  }

  lv_chart_set_range(ui_ThrustChart1, LV_CHART_AXIS_PRIMARY_Y, 0, maxThrust + 0.5 );
  if (standData.thrust > 0.5 ){
    thrustCurve.push_back(standData.thrust);
  }

  filteredData = applyMovingAverageFilter(thrustCurve, windowSize);

  rateOfChange = calculateInstantaneousRateOfChange(filteredData, filteredData.size());
  eventInProgress = detectEvent(rateOfChange, eventStartThreshold, eventStopThreshold);


  if (eventOccurred && !eventInProgress){
    if (done){
      return;
    }
    center_chart_series(ui_ThrustChart1, ser1);
    done = true;
    return;
  }

  if (eventInProgress && standData.thrust > 0.5){

    String max_string = String(maxThrust);
    _ui_label_set_property(ui_MaxThrustValue, _UI_LABEL_PROPERTY_TEXT, String(max_string + " g").c_str());
    lv_chart_set_next_value(ui_ThrustChart1, ser1, standData.thrust);
    String avg_string = String(avgThrust);
    String impulse_string = String (totalImpulse);
    
    avgThrust = std::accumulate(thrustCurve.begin(), thrustCurve.end(), 0.0) / thrustCurve.size();
    totalImpulse = std::accumulate(thrustCurve.begin(), thrustCurve.end(), 0.0) * 0.1;

    _ui_label_set_property(ui_AvgThrustValue, _UI_LABEL_PROPERTY_TEXT, String(avg_string + " g").c_str());
    _ui_label_set_property(ui_ImpulseValue, _UI_LABEL_PROPERTY_TEXT, String(impulse_string + " N-s").c_str());
  }
}


void loop() {
  if (millis() - debounce > 250 ){
    if (digitalRead(armingButton) == 0) {
      toggle_arm();
      debounce = millis();
    }
    if (digitalRead(ignitionButton) == 0) {
      fire();
      debounce = millis();
    }
  }
  lv_task_handler();
  // delay(5);
}

