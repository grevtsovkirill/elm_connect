#include <WiFi.h>
#include <WiFiClient.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

TFT_eSPI tft = TFT_eSPI();
#define TFT_BG TFT_BLACK
#define TFT_TEXT TFT_GREEN

const char* obdSSID = "WiFi-OBDII";
const char* obdPASS = "";
IPAddress obdIP(192, 168, 0, 10);
int obdPort = 35000;

WiFiClient client;

void showMessage(String msg, uint16_t color = TFT_TEXT) {
  tft.fillScreen(TFT_BG);
  tft.setTextColor(color, TFT_BG);
  tft.drawCentreString(msg, 160, 120, 2);
  Serial.println(msg);
}

String sendCommand(String cmd, int wait = 500) {
  client.print(cmd + "\r");
  delay(wait);
  String resp = "";
  while (client.available()) {
    resp += (char)client.read();
  }
  resp.trim();
  Serial.println("> " + cmd);
  Serial.println("< " + resp);
  return resp;
}

void scanSupportedPIDs() {
  Serial.println("\n--- Scanning supported PIDs ---");

  const char* pidGroups[] = {"0100", "0120", "0140", "0160", "0180"};
  for (int i = 0; i < 5; i++) {
    String cmd = pidGroups[i];
    String resp = sendCommand(cmd, 1000);

    int idx = resp.indexOf("41");
    if (idx < 0) continue;

    // Extract 4 bytes (8 hex chars) after "41 xx"
    String hexStr = resp.substring(idx + 4);
    hexStr.trim();

    // Parse bitmask
    uint32_t mask = strtoul(hexStr.substring(0, 8).c_str(), NULL, 16);
    Serial.printf("Supported PIDs for %s:\n", cmd);
    for (int bit = 0; bit < 32; bit++) {
      if (mask & (1UL << (31 - bit))) {
        int pidNum = 1 + bit + (i * 0x20);
        Serial.printf("PID %02X supported\n", pidNum);
      }
    }
    Serial.println();
  }
  Serial.println("--- End of PID scan ---\n");
}

void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(1);
  showMessage("Connecting Wi-Fi...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(obdSSID, obdPASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("Connected! IP: " + WiFi.localIP().toString());
  showMessage("Wi-Fi connected!");

  if (!client.connect(obdIP, obdPort)) {
    showMessage("ELM327 connect failed!", TFT_RED);
    while (1);
  }
  showMessage("ELM327 connected!");

  // Initialize ELM327
  sendCommand("ATZ", 1000);
  sendCommand("ATE0");
  sendCommand("ATL0");
  sendCommand("ATS0");
  sendCommand("ATH0");
  sendCommand("ATSP0");  // auto protocol

  // Scan supported PIDs
  scanSupportedPIDs();

  showMessage("Reading OBD...");
  delay(1000);
}




void loop() {
  // Request RPM (PID 010C)
  String rpmResp = sendCommand("010C");
  int rpm = -1;
  int idx = rpmResp.indexOf("410C");
  if (idx >= 0 && rpmResp.length() >= idx + 8) {
    String A_str = rpmResp.substring(idx + 4, idx + 6);
    String B_str = rpmResp.substring(idx + 6, idx + 8);
    int A = strtol(A_str.c_str(), NULL, 16);
    int B = strtol(B_str.c_str(), NULL, 16);
    rpm = ((A * 256) + B) / 4;
  }

  // Request coolant temperature (PID 0105)
  String tempResp = sendCommand("0105");
  int temp = -1;
  idx = tempResp.indexOf("4105");
  if (idx >= 0 && tempResp.length() >= idx + 6) {
    String A_str = tempResp.substring(idx + 4, idx + 6);
    int A = strtol(A_str.c_str(), NULL, 16);
    temp = A - 40;
  }

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawCentreString("RPM: " + String(rpm), 160, 100, 4);
  tft.drawCentreString("Temp: " + String(temp) + "C", 160, 140, 4);

  delay(2000); // update every 2 seconds
}

