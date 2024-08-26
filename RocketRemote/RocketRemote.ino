#include <esp_now.h>
#include <WiFi.h>
#include "ssFont.h"
#include "sFont.h"
#include "mFont.h"
#include "font18.h"

#include <TFT_eSPI.h>

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

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&tft);
TFT_eSprite graph = TFT_eSprite(&tft);
TFT_eSprite xaxis = TFT_eSprite(&tft);
TFT_eSprite yaxis = TFT_eSprite(&tft);

uint8_t broadcastAddress[] = { 0xb4, 0xe6, 0x2d, 0x69, 0xd6, 0xa0 };


#define gray 0x6B6D
#define blue 0x0967
#define orange 0xC260
#define purple 0x604D
#define green 0x1AE9
#define red 0xFF00


#define Key 14
#define arm 1
#define greenled 3
#define redled 2
#define FIRE 10

int gw = 204;
int gh = 130;
int gx = 105;
int gy = 20;
int curent = 0;
int graphVal = 1;
int lastval = 0;
int grid = 17;

// float graphVal = 0;


long lastMillis = 0;
long lastMsg = 0;
int fps = 0;
long timeDelta = 0;
int dataRate = 100;
bool armed = true;
long debounce = 0;
bool success;
float Max = 0;
int scale[] = {100, 250, 400, 750, 1200, 1500, 2000};
int scaleFactor = 1;

// callback function that will be executed when data is received
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
  memcpy(&standData, incomingData, sizeof(standData));
  graphVal = map(standData.thrust, 0, scale[scaleFactor], 0, 130);
  graphVal = constrain(graphVal, 0, 130);
  armed = standData.armed;
  if (standData.thrust > Max){
    Max = standData.thrust;
  }
  lastMsg = millis();
  draw_graph();
}

// Callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
  if (status != 0){
    success = false;
  }
}

void draw_graph() {
  // Draw our data points on the graph, then draw it again offset a bit so we have a 2px thick line.
  graph.drawLine(gw - 1, gh - lastval, gw - 1, gh - graphVal, TFT_RED);
  graph.drawLine(gw - 1, gh - lastval - 1, gw - 1, gh - graphVal - 1, TFT_RED);

  // Scroll everything on the graph to the left
  graph.scroll(-1, 0);

  // If we've scrolled 17 times (A nice multiple of our display size that makes for a decent grid)
  // Draw our vertical grid line
  grid++;
  if (grid >= 17) {
    graph.drawLine(gw - 1, 0, gw - 1, gh, gray);
    grid = 0;
  }

  // Every-other data point draw a line from our dava value to zero
  if (grid % 2 == 0) {
    graph.drawLine(gw - 1, gh - graphVal, gw - 1, gh, TFT_YELLOW);
  }

  // Draw our horizontal grid lines. 16.25 is the same nice multiple of our display size as 17 is above.
  for (int i = 1; i < 8; i++) {
    graph.drawPixel(gw - 1, gh - (i * 16.25), gray);
  }

  lastval = graphVal;
}

void draw_status() {
  // If it's been over a quarter of a second since our last message, call it disconnected.
  if (millis() - lastMsg > 250) {
    sprite.loadFont(font18);
    sprite.setTextColor(TFT_WHITE, TFT_RED);
    sprite.fillRoundRect(0, 0, 84, 20, 4, TFT_RED);
    sprite.drawString("Disconnected", 4, 4, 2);
    lastval = 0;
    standData.thrust = 0;
    sprite.unloadFont();
  } else {
    sprite.loadFont(ssFont);
    sprite.setTextColor(TFT_BLACK, TFT_GREEN);
    sprite.fillRoundRect(0, 0, 84, 20, 4, TFT_GREEN);
    sprite.drawString("Connected", 2, 5, 2);
    sprite.unloadFont();
  }
  sprite.loadFont(ssFont);
  if (standData.armed) {
    sprite.setTextColor(TFT_WHITE, TFT_RED);
    sprite.fillRoundRect(0, 22, 84, 20, 4, TFT_RED);
    sprite.drawString("ARMED!", 13, 28);
  } else {
    sprite.setTextColor(TFT_BLACK, TFT_GREEN);
    sprite.fillRoundRect(0, 22, 84, 20, 4, TFT_GREEN);
    sprite.drawString("Safe", 20, 28);
  }

  sprite.setTextColor(TFT_SILVER, purple);
  sprite.fillRoundRect(0, 150, 84, 20, 4, purple);
  sprite.drawString("FPS: " + String(fps), 4, 155);

  sprite.unloadFont();

  sprite.loadFont(font18);

  sprite.setTextColor(TFT_SILVER, green);
  sprite.fillRoundRect(0, 45, 84, 40, 4, green);
  sprite.drawString("Thrust:", 4, 49);
  sprite.drawString(String(standData.thrust) + "g", 4, 66);

  sprite.setTextColor(TFT_SILVER, orange);
  sprite.fillRoundRect(0, 88, 84, 40, 4, orange);
  sprite.drawString("Max Thrust:", 4, 92);
  sprite.drawString(String(Max) + "g", 4, 110);

  sprite.setTextColor(TFT_SILVER, blue);
  sprite.fillRoundRect(0, 130, 84, 20, 4, blue);
  sprite.drawString("Scale: " + String(scale[scaleFactor]), 4, 135);

  sprite.unloadFont();

  sprite.pushSprite(0, 0);
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

  pinMode(Key, INPUT_PULLUP);
  pinMode(arm, INPUT_PULLUP);
  pinMode(FIRE, INPUT_PULLUP);
  pinMode(redled, OUTPUT);
  pinMode(greenled, OUTPUT);
     //Turn on display power
   pinMode(15, OUTPUT);
   digitalWrite(15, HIGH);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  sprite.createSprite(85, 170);
  graph.createSprite(gw, gh);
  xaxis.createSprite(gw + 20, 20);
  yaxis.createSprite(20, 170);

  // Vertical Grid Lines
  for (int i = 1; i < 12; i++) {
    graph.drawLine(1 + (i * 17), gh, 1 + (i * 17), 0, gray);
  }

  // Horizontal Grid Lines
  for (int i = 1; i < 8; i++) {
    graph.drawLine(0, gh - (i * 16.25), gw, gh - (i * 16.25), gray);
  }

  graph.pushSprite(gx, gy);
  sprite.pushSprite(0, 0);

  // yaxis.drawLine(18, 20, 18, 20 + gh - 1, TFT_WHITE);
  yaxis.drawLine(18, 20, 18, 20 + gh, TFT_WHITE);
  yaxis.drawString("Thrust (grams)", gh / 3, 2);
  // yaxis.fillSprite(TFT_ORANGE);
  yaxis.pushSprite(86, 0);
  // yaxis.setPivot(-86, -150);
  // yaxis.pushRotated(85);

  xaxis.drawLine(0, 0, gw + 20, 0, TFT_WHITE);
  xaxis.loadFont(font18);
  xaxis.drawString("Time (" + String(1000 / dataRate) + "hz)", gw / 3, 2);
  xaxis.unloadFont();
  xaxis.pushSprite(104, 150);


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


void loop() {
  fps = 1000 / (millis() - lastMillis);
  lastMillis = millis();
  draw_status();
  graph.pushSprite(gx,gy);
  testStandCommands.fire = false;
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
    if (digitalRead(Key) == 0) {
      scaleFactor++;
      if (scaleFactor >= 7){
        scaleFactor = 0;
      }
      debounce = millis();
    }
    esp_now_send(broadcastAddress, (uint8_t *)&testStandCommands, sizeof(testStandCommands));
  }

}
