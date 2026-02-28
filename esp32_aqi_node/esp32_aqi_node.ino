#include <ArduinoJson.h>
#include <DHT.h>
#include <LiquidCrystal_I2C.h>
#include <WebSocketsClient.h>
#include <WiFi.h>
#include <Wire.h>
#include <esp_wifi.h>

// CRITICAL: Disable brownout detector to prevent resets on adapter power
#include "soc/rtc_cntl_reg.h"
#include "soc/soc.h"

// ==========================================
// 1. HARDWARE PIN DEFINITIONS
// ==========================================

// Sensors
const int PIN_MQ135 = 34;
const int PIN_MQ8 = 35;
const int PIN_MQ9 = 32;
const int PIN_DUST = 33;
const int PIN_DHT = 4; // DHT11 Data Pin

#define DHTTYPE DHT11

// Outputs & Indicators
const int PIN_FAN_PWM = 26;  // 12V Relay/MOSFET for Exhaust Fan
const int PIN_WIFI_LED = 18; // WiFi Status LED
const int PIN_ML_LED = 19;   // ML Alert LED

// ==========================================
// 2. CONFIGURATION & THRESHOLDS
// ==========================================

// Network Credentials
const char *ssid = "ESP32Dev";
const char *password = "12345678";

// WebSocket Server Settings (FastAPI Backend on Render)
const char *ws_host = "antigravity-aqi-backend.onrender.com";
const uint16_t ws_port = 443; // HTTPS/WSS port
const char *ws_path = "/ws/edge-node";

// Failsafe Thresholds (Raw analog threshold for testing)
const int MQ9_FAILSAFE_THRESHOLD = 3000;

// ==========================================
// 3. GLOBAL OBJECTS & STATE
// ==========================================

WebSocketsClient webSocket;
// Set I2C address to 0x27 or 0x3F for a 16x2 LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);
DHT dht(PIN_DHT, DHTTYPE);

// Timing Variables (Non-blocking logic)
unsigned long lastSensorRead = 0;
const unsigned long sensorInterval = 5000; // 5 seconds

unsigned long lastScreenChange = 0;
const unsigned long screenInterval = 3000; // 3 seconds
int currentScreen = 1;

// WiFi reconnection tracking
unsigned long lastWiFiRetry = 0;
const unsigned long wifiRetryInterval = 15000; // Retry WiFi every 15 seconds

// Sensor Values
int mq135Val = 0;
int mq8Val = 0;
int mq9Val = 0;
int dustVal = 0;
float temperature = 0.0;
float humidity = 0.0;

// Calibration Offsets (Baseline "Clean Air" readings)
int mq135Offset = 0;
int mq8Offset = 0;
int mq9Offset = 0;
int dustOffset = 0;

int currentFanSpeed = 0;

// ==========================================
// 4. FORWARD DECLARATIONS
// ==========================================
void webSocketEvent(WStype_t type, uint8_t *payload, size_t length);
void handleScreenUpdate();
void readAndSendSensors();
void controlFailSafe();
void calibrateSensors();

// ==========================================
// 5. SETUP FUNCTION
// ==========================================
void setup() {
  // CRITICAL: Disable brownout detector FIRST (before anything else)
  // This prevents the ESP32 from resetting when WiFi draws peak current
  // on unstable adapter power supplies
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  Serial.begin(115200);
  delay(5000); // CRITICAL: 5 second delay for adapter capacitor charging &
               // voltage stabilization
  Serial.println("\n\n=============================");
  Serial.println("Antigravity AQI Node v1.0");
  Serial.println("=============================");

  // Initialize Output Pins
  pinMode(PIN_WIFI_LED, OUTPUT);
  pinMode(PIN_ML_LED, OUTPUT);
  pinMode(PIN_FAN_PWM, OUTPUT);

  // Ensure fan is off initially
  analogWrite(PIN_FAN_PWM, 0);
  digitalWrite(PIN_ML_LED, LOW);
  digitalWrite(PIN_WIFI_LED, LOW);

  // Initialize LCD with error handling
  Serial.println("[LCD] Initializing...");
  Wire.begin();
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("AQI Node Boot...");
  Serial.println("[LCD] OK");

  // Initialize DHT11 Temperature & Humidity Sensor
  Serial.println("[DHT11] Initializing...");
  dht.begin();
  Serial.println("[DHT11] OK");

  // Connect to WiFi with TIMEOUT (max 30 seconds)
  Serial.printf("[WiFi] Connecting to: %s\n", ssid);
  lcd.setCursor(0, 1);
  lcd.print("WiFi connecting..");

  // Clean WiFi state before connecting (prevents stale connection issues)
  WiFi.persistent(
      false); // Don't save WiFi credentials to flash (reduces power spikes)
  WiFi.disconnect(true); // Force disconnect any stale connection
  delay(1000);           // Let the radio fully reset
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true); // Auto-reconnect if connection drops

  // CRITICAL: Reduce WiFi TX power to prevent current spikes on adapter power
  // Default is 20dBm (100mW) which causes 300-500mA spikes.
  // 8dBm (~6mW) is enough for short-range home WiFi and much more stable.
  WiFi.setTxPower(WIFI_POWER_8_5dBm);

  // Disable modem sleep to prevent erratic power draw patterns
  esp_wifi_set_ps(WIFI_PS_NONE);

  WiFi.begin(ssid, password);

  int attempts = 0;
  const int maxAttempts =
      60; // 60 x 500ms = 30 seconds (more time for adapter boot)
  while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
    delay(500);
    Serial.print(".");
    digitalWrite(PIN_WIFI_LED, !digitalRead(PIN_WIFI_LED));
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[WiFi] Connected! IP: %s\n",
                  WiFi.localIP().toString().c_str());
    digitalWrite(PIN_WIFI_LED, HIGH);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Connected");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP());
  } else {
    Serial.println("\n[WiFi] FAILED! Running in offline/failsafe mode.");
    digitalWrite(PIN_WIFI_LED, LOW);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi FAILED!");
    lcd.setCursor(0, 1);
    lcd.print("Offline Mode");
  }

  // Initialize WebSocket connection (will auto-reconnect)
  Serial.printf("[WS] Target: wss://%s:%d%s\n", ws_host, ws_port, ws_path);
  webSocket.beginSSL(ws_host, ws_port, ws_path);
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);

  delay(1500);
  lcd.clear();
  Serial.println("[BOOT] Setup complete. Entering main loop.");
}

// ==========================================
// 6. MAIN LOOP (NON-BLOCKING)
// ==========================================
void loop() {
  // Keep the WebSocket client active
  webSocket.loop();

  unsigned long currentMillis = millis();

  // Task 1: Read sensors and transmit data every 5 seconds
  if (currentMillis - lastSensorRead >= sensorInterval) {
    lastSensorRead = currentMillis;
    readAndSendSensors();
  }

  // Task 2: Handle Rolling OLED/LCD Screen Logic every 3 seconds
  if (currentMillis - lastScreenChange >= screenInterval) {
    lastScreenChange = currentMillis;
    currentScreen = (currentScreen == 1) ? 2 : 1; // Toggle screen state
    handleScreenUpdate();
  }

  // Task 3: Fail-safe check + WiFi auto-reconnect
  if (WiFi.status() != WL_CONNECTED) {
    digitalWrite(PIN_WIFI_LED, LOW); // Indicate connection lost
    controlFailSafe();

    // Auto-reconnect WiFi every 15 seconds
    if (currentMillis - lastWiFiRetry >= wifiRetryInterval) {
      lastWiFiRetry = currentMillis;
      Serial.println("[WiFi] Attempting reconnect...");
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("WiFi Reconnect..");
      WiFi.disconnect(true);
      delay(500);
      WiFi.begin(ssid, password);
    }
  } else {
    digitalWrite(PIN_WIFI_LED, HIGH); // Confirm connection active
  }
}

// ==========================================
// 7. SENSOR READING & TRANSMISSION
// ==========================================
void readAndSendSensors() {
  // Read raw analog values (0-4095 on ESP32)
  int rawMq135 = analogRead(PIN_MQ135);
  int rawMq8 = analogRead(PIN_MQ8);
  int rawMq9 = analogRead(PIN_MQ9);
  int rawDust = analogRead(PIN_DUST);

  // Apply calibration offsets (floor at 0)
  mq135Val = max(0, rawMq135 - mq135Offset);
  mq8Val = max(0, rawMq8 - mq8Offset);
  mq9Val = max(0, rawMq9 - mq9Offset);
  dustVal = max(0, rawDust - dustOffset);

  // Read DHT11 Temperature & Humidity
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t))
    temperature = t;
  if (!isnan(h))
    humidity = h;

  // Allocate JSON document
  StaticJsonDocument<300> doc;
  doc["mq135"] = mq135Val;
  doc["mq8"] = mq8Val;
  doc["mq9"] = mq9Val;
  doc["dust"] = dustVal;
  doc["temperature"] = temperature;
  doc["humidity"] = humidity;
  doc["fan_speed"] = currentFanSpeed;

  String payload;
  serializeJson(doc, payload);

  // Send over WebSocket if connected
  if (webSocket.isConnected()) {
    webSocket.sendTXT(payload);
    Serial.println("Transmitted: " + payload);
  } else {
    Serial.println("WS Disconnected. Local Data: " + payload);
  }
}

// ==========================================
// 8. LOCAL FAIL-SAFE CONTROLLER
// ==========================================
void controlFailSafe() {
  // If MQ9 (CO/Combustible gases) exceeds hardcoded danger threshold
  if (mq9Val > MQ9_FAILSAFE_THRESHOLD) {
    currentFanSpeed = 255; // 100% Duty Cycle
    analogWrite(PIN_FAN_PWM, currentFanSpeed);

    digitalWrite(PIN_ML_LED, HIGH); // Flash Alert LED
    Serial.println("WARNING: Local Failsafe Activated! Fan 100%");
  } else {
    // Return to baseline if safe
    currentFanSpeed = 0;
    analogWrite(PIN_FAN_PWM, currentFanSpeed);
    digitalWrite(PIN_ML_LED, LOW);
  }
}

// ==========================================
// 9. LOCAL UI (LCD ROLLING SCREENS)
// ==========================================
void handleScreenUpdate() {
  lcd.clear();
  if (currentScreen == 1) {
    // Screen 1: Gas Stats
    lcd.setCursor(0, 0);
    lcd.print("Q135:");
    lcd.print(mq135Val);
    lcd.print(" Q8:");
    lcd.print(mq8Val);
    lcd.setCursor(0, 1);
    lcd.print("Q9:");
    lcd.print(mq9Val);
    lcd.print(" D:");
    lcd.print(dustVal);
  } else {
    // Screen 2: Temp, Humidity & Fan Status
    lcd.setCursor(0, 0);
    lcd.print("T:");
    lcd.print(temperature, 1);
    lcd.print("C H:");
    lcd.print(humidity, 0);
    lcd.print("%");
    lcd.setCursor(0, 1);

    int fanPct = map(currentFanSpeed, 0, 255, 0, 100);
    lcd.print("Fan:");
    lcd.print(fanPct);
    lcd.print("% ");
    if (WiFi.status() == WL_CONNECTED) {
      lcd.print("[ON]");
    } else {
      lcd.print("[XX]");
    }
  }
}

// ==========================================
// 10. WEBSOCKET EVENT HANDLER (REAL-TIME ML COMMANDS)
// ==========================================
void webSocketEvent(WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
  case WStype_DISCONNECTED:
    Serial.println("[WebSocket] Disconnected from server.");
    break;

  case WStype_CONNECTED:
    Serial.printf("[WebSocket] Connected to: %s\n", payload);
    // Optional: Send an initial handshake packet
    webSocket.sendTXT(
        "{\"status\": \"device_connected\", \"device_id\": \"esp32_edge_01\"}");
    break;

  case WStype_TEXT: {
    Serial.printf("[WebSocket] Received Payload: %s\n", payload);

    // Parse incoming JSON from ML Backend
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, payload, length);

    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      return;
    }

    // 1. Handle Real-Time ML Fan Control
    if (doc.containsKey("fan_speed")) {
      currentFanSpeed = doc["fan_speed"];
      currentFanSpeed = constrain(currentFanSpeed, 0, 255);

      analogWrite(PIN_FAN_PWM, currentFanSpeed);
      Serial.printf("ML Command -> Fan updated to: %d\n", currentFanSpeed);

      // Toggle ML Alert LED to indicate automated remote action
      if (currentFanSpeed > 0) {
        digitalWrite(PIN_ML_LED, HIGH);
      } else {
        digitalWrite(PIN_ML_LED, LOW);
      }
    }

    // 2. Handle Calibration Command (The "Winning Move" logic)
    if (doc.containsKey("command") && doc["command"] == "calibrate") {
      calibrateSensors();
    }
  } break;

  case WStype_BIN:
  case WStype_PING:
  case WStype_PONG:
  case WStype_ERROR:
    break;
  }
}

// ==========================================
// 11. CALIBRATION LOGIC
// ==========================================
void calibrateSensors() {
  Serial.println("Calibration triggered remotely...");

  // Provide immediate local UI feedback
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Calibrating...");
  lcd.setCursor(0, 1);
  lcd.print("Please wait...");

  // Calculate offsets based on current raw readings
  // Assuming calibration is triggered ONLY when placed in clean air
  // We leave a small buffer (e.g., 200) to act as the "zero" floor.
  int cleanAirFloor = 200;

  mq135Offset = analogRead(PIN_MQ135) - cleanAirFloor;
  mq8Offset = analogRead(PIN_MQ8) - cleanAirFloor;
  mq9Offset = analogRead(PIN_MQ9) - cleanAirFloor;
  dustOffset = analogRead(PIN_DUST) - cleanAirFloor;

  // Prevent negative offsets
  mq135Offset = max(0, mq135Offset);
  mq8Offset = max(0, mq8Offset);
  mq9Offset = max(0, mq9Offset);
  dustOffset = max(0, dustOffset);

  Serial.println("Calibration complete. New Offsets set.");

  // Pause the UI roll briefly so the user sees the calibration status
  lastScreenChange = millis() + 2000;
}
