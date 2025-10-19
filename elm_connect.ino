/* Minimal ELM327 Bluetooth test for ESP32 + CYD (2.8" cheap yellow display)
   Based on your working TFT + touch sketch.

   Instructions:
   - Pair your ELM327 Bluetooth dongle with the host (optional, but helpful).
   - Replace ELM_BT_ADDR with your dongle's Bluetooth MAC address (format "AA:BB:CC:DD:EE:FF").
   - Flash, open Serial Monitor at 115200, then tap the screen to start a connection attempt.
   - Observed behavior: connect status shown on TFT; any reply from ELM327 printed on Serial + TFT.
*/

#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include "BluetoothSerial.h"

TFT_eSPI tft = TFT_eSPI();

// Touchscreen pins (these match your working setup)
// #define XPT2046_IRQ 36   // T_IRQ
// #define XPT2046_CS  33   // T_CS
// #define XPT2046_MOSI 23  // shared with display
// #define XPT2046_MISO 19  // shared with display
// #define XPT2046_CLK  18  // shared with display
#define XPT2046_IRQ 36   // T_IRQ
#define XPT2046_MOSI 32  // T_DIN
#define XPT2046_MISO 39  // T_OUT
#define XPT2046_CLK 25   // T_CLK
#define XPT2046_CS 33    // T_CS

SPIClass touchscreenSPI = SPIClass(VSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);

// Bluetooth (ELM327)
BluetoothSerial SerialBT;
// Replace with your ELM327 Bluetooth MAC address
const char *ELM_BT_ADDR = "00:10:CC:4F:36:03"; // <<< REPLACE with your dongle MAC

// UI constants
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define FONT_SIZE 2

bool btConnected = false;
String lastResponse = "";

void printToTFT(const String &line, int y) {
  tft.setTextDatum(MC_DATUM);
  tft.drawString(line, SCREEN_WIDTH/2, y, FONT_SIZE);
}

void showStatus(const String &title, const String &line2) {
  tft.fillScreen(TFT_WHITE);
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  printToTFT(title, 30);
  printToTFT(line2, 80);
  printToTFT("Tap screen to (re)try", 200);
}

void setup() {
  Serial.begin(115200);

  // initialize touchscreen SPI and touchscreen object
  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touchscreen.begin(touchscreenSPI);
  touchscreen.setRotation(1);

  // TFT init
  tft.init();
  tft.setRotation(1);
  // Backlight (some CYD boards require this)
  pinMode(32, OUTPUT);
  digitalWrite(32, HIGH);

  tft.fillScreen(TFT_WHITE);
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  printToTFT("ELM327 BT Test", 30);
  printToTFT("Tap screen to connect", 80);

  // Start Bluetooth subsystem (no device name needed for client)
  if(!SerialBT.begin("ESP32_OBD_CLIENT", true)) {
    Serial.println("SerialBT begin failed!");
  } else {
    Serial.println("SerialBT started");
  }
}

unsigned long lastPoll = 0;
void loop() {
  // When screen tapped, attempt connection
  if (touchscreen.tirqTouched() && touchscreen.touched()) {
    // small debounce
    TS_Point p = touchscreen.getPoint();
    delay(50);
    Serial.println("Screen tapped -> attempting BT connect...");
    showStatus("Connecting...", String("Trying: ") + ELM_BT_ADDR);

    // Attempt to connect to remote BT device (ELM327). Replace with correct MAC.
    btConnected = SerialBT.connect(String(ELM_BT_ADDR));
    if (btConnected) {
      Serial.println("BT connected!");
      showStatus("Connected", String("To: ") + ELM_BT_ADDR);
      // Send a simple AT command to verify
      SerialBT.print("ATZ\r");
      Serial.println("Sent: ATZ");
      lastPoll = millis();
    } else {
      Serial.println("BT connect failed");
      showStatus("Connect failed", "Tap to retry");
    }
  }

  // If connected, read data from ELM327 and display it
  if (btConnected && SerialBT.connected()) {
    // read any available data and aggregate a line
    while (SerialBT.available()) {
      String s = SerialBT.readStringUntil('\r'); // ELM replies often end with \r
      s.trim();
      if (s.length() > 0) {
        Serial.print("ELM> "); Serial.println(s);
        lastResponse = s;
        // display last response on TFT
        tft.fillRect(0, 100, SCREEN_WIDTH, 60, TFT_WHITE);
        tft.setTextColor(TFT_BLACK, TFT_WHITE);
        printToTFT("Reply:", 100);
        printToTFT(s, 120);
      }
    }

    // Keep the connection alive: if no data for 5s, poll with "AT@1" or "ATI"
    if (millis() - lastPoll > 5000) {
      SerialBT.print("ATI\r"); // ask for device info
      lastPoll = millis();
    }

    // If remote disconnected, update status
    if (!SerialBT.connected()) {
      Serial.println("BT disconnected!");
      btConnected = false;
      showStatus("Disconnected", "Tap to retry");
    }
  }

  // Periodically also show latest serial output on TFT footer
  static unsigned long footerUpdate = 0;
  if (millis() - footerUpdate > 1000) {
    tft.fillRect(0, 180, SCREEN_WIDTH, 40, TFT_WHITE);
    tft.setTextColor(TFT_BLACK, TFT_WHITE);
    printToTFT("Last: " + lastResponse, 200);
    footerUpdate = millis();
  }
}
