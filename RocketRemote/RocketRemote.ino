#include <esp_now.h>
#include <WiFi.h>
#include <TFT_eSPI.h>

// Structure example to receive data
// Must match the sender structure
typedef struct struct_message {
    char a[32];
    int b;
    float c;
    bool d;
} struct_message;

// Create a struct_message called myData
struct_message myData;

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&tft);
TFT_eSprite graph = TFT_eSprite(&tft);

#define gray 0x6B6D
#define blue 0x0967
#define orange 0xC260
#define purple 0x604D
#define green 0x1AE9
#define red 0xFF00

int gw=210;
int gh=150;
int gx=105;
int gy=10;
int curent=0;
int graphVal = 1;
int delta = 1;
int grid = 0;

// callback function that will be executed when data is received
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&myData, incomingData, sizeof(myData));
  Serial.print("Bytes received: ");
  Serial.println(len);
  Serial.print("Char: ");
  Serial.println(myData.a);
  Serial.print("Int: ");
  Serial.println(myData.b);
  Serial.print("Float: ");
  Serial.println(myData.c);
  Serial.print("Bool: ");
  Serial.println(myData.d);
  Serial.println();
}

void draw_graph() {
  graph.drawFastVLine(gx, gh-graphVal, 1, TFT_YELLOW);
  // graph.pushSprite(gx,gy);
  // graphVal += delta;
  // // If the value reaches a limit, then change delta of value
  // if (graphVal >= gh)     delta = -1;  // ramp down value
  // else if (graphVal <= 1) delta = +1;  // ramp up value

  // graph.scroll(-1,0);
  // grid++;
  // if (grid >= 10)
  // { // Draw a vertical line if we have scrolled 10 times (10 pixels)
  //   grid = 0;
  //   graph.drawFastVLine(gx, gh, gh, TFT_NAVY); // draw line on graph
  // }
  // else
  // { // Otherwise draw points spaced 10 pixels for the horizontal grid lines
  //   for (int p = 20; p <= 170; p += 10) graph.drawPixel(gx, p, TFT_NAVY);
  // }
}
 
void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);

  pinMode(15,OUTPUT);
  digitalWrite(15,1);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  sprite.createSprite(320,170);
  graph.createSprite(gw, gh);

  sprite.setTextColor(TFT_WHITE,TFT_RED);
  sprite.fillRoundRect(1,1,85,20,3,TFT_RED);
  sprite.drawString("Disconnected",4,4,2);
  sprite.pushSprite(0,0);

  for(int i=1;i<12;i++){
    graph.drawLine((i*17),gh,(i*17),0,gray);
  }
  for(int i=1;i<6;i++){
    graph.drawLine(0,gh-(i*17),0+gw,gh-(i*17),gray);
  }
  graph.drawLine(0,gy,0+gw,gy,TFT_WHITE);
  graph.drawLine(0,gy,0,gy-gh,TFT_WHITE);
  graph.pushSprite(gx,gy);



  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  
  // Once ESPNow is successfully Init, we will register for recv CB to
  // get recv packer info
  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));
}
 
void loop() {
  draw_graph();
}
