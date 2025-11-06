#include <WiFi.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

TFT_eSPI tft = TFT_eSPI();

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

// --- Touchscreen pins (CYD defaults) ---
#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33

SPIClass touchscreenSPI = SPIClass(VSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);

// --- Button geometry ---
#define BTN_W 220
#define BTN_H 60
#define BTN_RADIUS 10

struct Button {
  int x, y;
  String label;
};

Button btnConnect = { (SCREEN_WIDTH - BTN_W) / 2, 60, "Connect to ELM327 Wi-Fi" };
Button btnSettings = { (SCREEN_WIDTH - BTN_W) / 2, 150, "Settings" };

// --- Network settings ---
const char* obdSSID = "WiFi-OBDII";
const char* obdPASS = "";
const IPAddress obdIP(192, 168, 0, 10);   // common ELM327 Wi-Fi IP
int obdPort = 35000; 

// --- Button draw helpers ---
void drawButton(Button b, uint16_t color, uint16_t textColor) {
  tft.fillRoundRect(b.x, b.y, BTN_W, BTN_H, BTN_RADIUS, color);
  tft.drawRoundRect(b.x, b.y, BTN_W, BTN_H, BTN_RADIUS, TFT_BLACK);
  tft.setTextColor(textColor, color);
  tft.drawCentreString(b.label, SCREEN_WIDTH / 2, b.y + (BTN_H / 2) - 8, 2);
}

bool isTouched(Button b, int tx, int ty) {
  return (tx >= b.x && tx <= b.x + BTN_W && ty >= b.y && ty <= b.y + BTN_H);
}

// --- Display message helper ---
void showMessage(String msg, uint16_t color = TFT_BLACK) {
  tft.fillRect(0, 0, SCREEN_WIDTH, 30, TFT_WHITE);
  tft.setTextColor(color, TFT_WHITE);
  tft.drawCentreString(msg, SCREEN_WIDTH / 2, 5, 2);
  Serial.println(msg);
}

// --- Wi-Fi connect + ping ---
void connectAndPing() {
  showMessage("Connecting to ELM327 Wi-Fi...");
  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);

  // Attempt open network connection first
  Serial.println("Trying open Wi-Fi connection...");
  WiFi.begin(obdSSID);

  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 10000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  // If open network failed and we have a password, try WPA
  if (WiFi.status() != WL_CONNECTED && strlen(obdPASS) > 0) {
    Serial.println("Retrying with password...");
    WiFi.begin(obdSSID, obdPASS);
    startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 10000) {
      delay(500);
      Serial.print(".");
    }
    Serial.println();
  }

  if (WiFi.status() == WL_CONNECTED) {
    IPAddress localIP = WiFi.localIP();
    showMessage("Connected! IP: " + localIP.toString(), TFT_BLUE);
    delay(1000);
    showMessage("Testing OBDII link...", TFT_BLACK);

    // --- Minimal "ping" replacement ---
    WiFiClient client;
    if (client.connect(obdIP, obdPort)) {   // ELM327 usually on port 23
      showMessage("ELM327 reachable!", TFT_GREEN);
      Serial.println("Connected to ELM327 successfully!");
      client.stop();
    } else {
      showMessage("Cannot reach ELM327.", TFT_RED);
      Serial.println("Failed to connect to ELM327 .");
    }
  } else {
    showMessage("Wi-Fi connection failed!", TFT_RED);
    Serial.println("Wi-Fi connection failed!");
  }

  delay(3000);
  showMessage("Ready");
}



// --- Setup ---
void setup() {
  Serial.begin(115200);
  delay(500);

  // Touch init
  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touchscreen.begin(touchscreenSPI);
  touchscreen.setRotation(1);

  // Display init
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_WHITE);
  tft.drawCentreString("CYD Wi-Fi ELM327 Interface", SCREEN_WIDTH / 2, 10, 2);
  drawButton(btnConnect, TFT_CYAN, TFT_BLACK);
  drawButton(btnSettings, TFT_YELLOW, TFT_BLACK);

  showMessage("Touch any button...");
}

// --- Main loop ---
void loop() {
  if (touchscreen.tirqTouched() && touchscreen.touched()) {
    TS_Point p = touchscreen.getPoint();
    int x = map(p.x, 200, 3700, 1, SCREEN_WIDTH);
    int y = map(p.y, 240, 3800, 1, SCREEN_HEIGHT);

    if (isTouched(btnConnect, x, y)) {
      drawButton(btnConnect, TFT_BLUE, TFT_WHITE);
      connectAndPing();
      drawButton(btnConnect, TFT_CYAN, TFT_BLACK);
    } else if (isTouched(btnSettings, x, y)) {
      drawButton(btnSettings, TFT_ORANGE, TFT_WHITE);
      showMessage("Settings (placeholder)", TFT_BLACK);
      delay(1000);
      drawButton(btnSettings, TFT_YELLOW, TFT_BLACK);
      showMessage("Ready");
    }

    delay(200);
  }
}
