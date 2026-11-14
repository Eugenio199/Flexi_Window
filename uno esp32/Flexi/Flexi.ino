
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

// ==========================================
// ESP32 SMART WINDOW
// Automatic Day/Night + Rain Protection
// + Restore Previous State After Rain
// + Manual Button + L298N DC Motor
// ==========================================

// ---------- GPIO PINS ----------
constexpr uint8_t MOTOR_IN1 = 5;
constexpr uint8_t MOTOR_IN2 = 4;
constexpr uint8_t MOTOR_ENA = 13;

constexpr uint8_t LDR_PIN = 36;       // VP / ADC1_CH0
constexpr uint8_t RAIN_PIN = 14;      // LM393 rain sensor DO
constexpr uint8_t BUTTON_PIN = 25;    // Button connects GPIO25 to 3.3V

// ---------- SENSOR SETTINGS ----------

// REVERSED RAIN SENSOR:
// false = HIGH means wet, LOW means dry
// true  = LOW means wet, HIGH means dry
// Change this if Serial Monitor shows the opposite of reality.
constexpr bool RAIN_ACTIVE_LOW = true;

// true: brighter light gives higher ADC readings
// false: brighter light gives lower ADC readings
constexpr bool BRIGHTER_GIVES_HIGHER_ADC = false;

constexpr int DAYLIGHT_OPEN_THRESHOLD = 2400;
constexpr int NIGHT_CLOSE_THRESHOLD = 1800;

// ---------- MOTOR SETTINGS ----------
constexpr uint32_t MOTOR_RUN_MS = 90;

// Enter the 2.4 GHz Wi-Fi credentials used by your phone/computer.
const char* WIFI_SSID = "404_error!";
const char* WIFI_PASSWORD = "Cuaresma05.";

// This setup network stays available even if router Wi-Fi is unavailable.
const char* SETUP_AP_SSID = "FlexiWindow-Setup";
const char* SETUP_AP_PASSWORD = "FlexiWin-49281";

// ---------- BUTTON SETTINGS ----------
constexpr uint32_t BUTTON_DEBOUNCE_MS = 35;

// ---------- RAIN FILTER SETTINGS ----------
constexpr uint32_t RAIN_CONFIRM_MS = 1000;
constexpr uint32_t DRY_CONFIRM_MS = 2000;

// ---------- WINDOW STATE ----------
enum class WindowState : uint8_t {
  Closed,
  Open
};

WindowState windowState = WindowState::Closed;

bool motorRunning = false;
bool movingTowardOpen = false;

bool desiredOpen = false;
bool autoDay = false;
bool autoStateInitialized = false;
bool manualOverrideActive = false;
bool autoDayNightEnabled = true;
bool autoRainEnabled = true;
bool manualModeEnabled = false;

WebServer server(80);
bool webServerStarted = false;
bool routerConnectionReported = false;
uint32_t lastWiFiRetryAt = 0;
int currentLightReading = 0;

// Rain protection state
bool rainActive = false;
bool rainCandidate = false;
bool rainCandidateInitialized = false;

bool previousWindowStateSaved = false;
bool previousWindowWasOpen = false;

uint32_t rainCandidateSince = 0;

// Button uses INPUT_PULLDOWN.
// Released = LOW, pressed = HIGH.
bool lastRawButton = LOW;
bool stableButton = LOW;

uint32_t lastButtonChangeAt = 0;
uint32_t motorStartedAt = 0;

// ==========================================
// WI-FI API
// ==========================================

String buildStateJson() {
  String json = "{";
  json += "\"state\":\"";
  json += windowState == WindowState::Open ? "OPEN" : "CLOSED";
  json += "\",\"moving\":";
  json += motorRunning ? "true" : "false";
  json += ",\"targetOpen\":";
  json += desiredOpen ? "true" : "false";
  json += ",\"light\":";
  json += String(currentLightReading);
  json += ",\"rainWet\":";
  json += rainActive ? "true" : "false";
  json += ",\"rainProtection\":";
  json += (autoRainEnabled && rainActive) ? "true" : "false";
  json += ",\"autoDayNight\":";
  json += autoDayNightEnabled ? "true" : "false";
  json += ",\"autoRain\":";
  json += autoRainEnabled ? "true" : "false";
  json += ",\"manualOverride\":";
  json += manualModeEnabled ? "true" : "false";
  json += "}";
  return json;
}

void sendApiResponse(int statusCode, const String& body) {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Cache-Control", "no-store");
  server.send(statusCode, "application/json", body);
}

void handleStateRequest() {
  sendApiResponse(200, buildStateJson());
}

void handleModeRequest() {
  if (!server.hasArg("dayNight") ||
      !server.hasArg("rain") ||
      !server.hasArg("manual")) {
    sendApiResponse(400, "{\"error\":\"dayNight, rain, and manual are required\"}");
    return;
  }

  manualModeEnabled = server.arg("manual") == "1";
  autoDayNightEnabled =
      !manualModeEnabled && server.arg("dayNight") == "1";
  autoRainEnabled =
      !manualModeEnabled && server.arg("rain") == "1";

  if (autoDayNightEnabled) {
    manualOverrideActive = false;
  }

  sendApiResponse(200, buildStateJson());
}

void handleControlRequest() {
  if (!server.hasArg("action") ||
      (server.arg("action") != "open" &&
       server.arg("action") != "close")) {
    sendApiResponse(400, "{\"error\":\"action must be open or close\"}");
    return;
  }

  if (!manualModeEnabled) {
    sendApiResponse(403, "{\"error\":\"Enable Manual Override before controlling the window\"}");
    return;
  }

  if (autoRainEnabled && rainActive) {
    sendApiResponse(409, "{\"error\":\"Rain protection is active\"}");
    return;
  }

  if (motorRunning) {
    sendApiResponse(409, "{\"error\":\"Window motor is already moving\"}");
    return;
  }

  desiredOpen = server.arg("action") == "open";
  manualOverrideActive = true;
  sendApiResponse(200, buildStateJson());
}

void startWebServer() {
  server.on("/api/state", HTTP_GET, handleStateRequest);
  server.on("/api/mode", HTTP_GET, handleModeRequest);
  server.on("/api/control", HTTP_GET, handleControlRequest);
  server.begin();
  webServerStarted = true;

  Serial.print("Controller API ready at http://");
  Serial.println(WiFi.softAPIP());
  Serial.println("Join the setup Wi-Fi and enter the address above.");
}

// ==========================================
// LIGHT SENSOR FUNCTIONS
// ==========================================

bool isDaylight(int reading) {
  if (BRIGHTER_GIVES_HIGHER_ADC) {
    return reading >= DAYLIGHT_OPEN_THRESHOLD;
  }

  return reading <= DAYLIGHT_OPEN_THRESHOLD;
}

bool isNight(int reading) {
  if (BRIGHTER_GIVES_HIGHER_ADC) {
    return reading <= NIGHT_CLOSE_THRESHOLD;
  }

  return reading >= NIGHT_CLOSE_THRESHOLD;
}

// ==========================================
// RAIN SENSOR FUNCTIONS
// ==========================================

bool readRainSensor() {
  const int reading = digitalRead(RAIN_PIN);

  // Convert electrical level into a wet/dry meaning.
  return reading == (RAIN_ACTIVE_LOW ? LOW : HIGH);
}

// ==========================================
// MOTOR CONTROL
// ==========================================

void stopMotor() {
  digitalWrite(MOTOR_ENA, LOW);
  digitalWrite(MOTOR_IN1, LOW);
  digitalWrite(MOTOR_IN2, LOW);

  motorRunning = false;
}

void startMotion(bool opening) {
  movingTowardOpen = opening;

  // Reverse these levels if your motor direction is reversed.
  digitalWrite(MOTOR_IN1, opening ? HIGH : LOW);
  digitalWrite(MOTOR_IN2, opening ? LOW : HIGH);

  digitalWrite(MOTOR_ENA, HIGH);

  motorStartedAt = millis();
  motorRunning = true;

  Serial.println(opening
                   ? "Motor: OPENING window"
                   : "Motor: CLOSING window");
}

// ==========================================
// RAIN STATE HANDLING
// ==========================================

void updateRainState(bool rawWet, uint32_t now) {
  // Initialize the candidate on the first reading.
  if (!rainCandidateInitialized) {
    rainCandidate = rawWet;
    rainCandidateSince = now;
    rainCandidateInitialized = true;
  }

  // Start timing whenever the sensor reading changes.
  if (rawWet != rainCandidate) {
    rainCandidate = rawWet;
    rainCandidateSince = now;
  }

  // Require a stable wet/dry reading to prevent flickering.
  uint32_t confirmTime =
      rainCandidate ? RAIN_CONFIRM_MS : DRY_CONFIRM_MS;

  if (rainCandidate != rainActive &&
      (uint32_t)(now - rainCandidateSince) >= confirmTime) {

    rainActive = rainCandidate;

    if (rainActive) {
      // Save the target state before rain protection takes over.
      // If the window is currently moving, remember its destination.
      previousWindowWasOpen =
          motorRunning ? movingTowardOpen
                       : (windowState == WindowState::Open);

      previousWindowStateSaved = true;

      Serial.println("RAIN CONFIRMED: closing window.");
      Serial.println(
        previousWindowWasOpen
          ? "Saved state: OPEN"
          : "Saved state: CLOSED"
      );

    } else {
      Serial.println("RAIN ENDED: restoring previous window state.");

      if (previousWindowStateSaved) {
        desiredOpen = previousWindowWasOpen;
      }

      previousWindowStateSaved = false;
    }
  }
}

// ==========================================
// SETUP
// ==========================================

void setup() {
  Serial.begin(115200);

  pinMode(MOTOR_IN1, OUTPUT);
  pinMode(MOTOR_IN2, OUTPUT);
  pinMode(MOTOR_ENA, OUTPUT);

  // LM393 digital output.
  // Use the sensor module's normal output wiring.
  pinMode(RAIN_PIN, INPUT_PULLUP);

  pinMode(BUTTON_PIN, INPUT_PULLDOWN);

  analogReadResolution(12);

  stopMotor();

  Serial.println();
  Serial.println("================================");
  Serial.println(" ESP32 SMART WINDOW STARTED");
  Serial.println(" Automatic day/night control");
  Serial.println(" Rain protection enabled");
  Serial.println(" Previous-state restoration");
  Serial.println(" Manual push-button enabled");
  Serial.println("================================");

  Serial.println(
    "Window position is estimated using motor timing."
  );

  Serial.print("Rain active level: ");
  Serial.println(RAIN_ACTIVE_LOW ? "LOW" : "HIGH");

  WiFi.mode(WIFI_AP_STA);
  if (WiFi.softAP(SETUP_AP_SSID, SETUP_AP_PASSWORD)) {
    Serial.print("Setup Wi-Fi: ");
    Serial.println(SETUP_AP_SSID);
    Serial.print("Setup Wi-Fi password: ");
    Serial.println(SETUP_AP_PASSWORD);
  } else {
    Serial.println("ERROR: Could not start setup Wi-Fi.");
  }

  startWebServer();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  lastWiFiRetryAt = millis();
  Serial.println("Attempting to connect to the configured router Wi-Fi.");
}

// ==========================================
// MAIN LOOP
// ==========================================

void loop() {
  const uint32_t now = millis();

  // ---------- READ SENSORS ----------
  const int lightLevel = analogRead(LDR_PIN);
  const bool rawWet = readRainSensor();
  currentLightReading = lightLevel;

  // ---------- FILTER RAIN SENSOR ----------
  updateRainState(rawWet, now);

  // ---------- AUTOMATIC DAY/NIGHT ----------
  bool nextAutoDay = autoDay;

  if (isDaylight(lightLevel)) {
    nextAutoDay = true;
  } else if (isNight(lightLevel)) {
    nextAutoDay = false;
  }

  if (!autoStateInitialized) {
    autoDay = nextAutoDay;
    autoStateInitialized = true;

    desiredOpen = autoDay;

    Serial.println(
      autoDay ? "Automatic mode: DAY"
              : "Automatic mode: NIGHT"
    );

  } else if (nextAutoDay != autoDay) {
    autoDay = nextAutoDay;

    // Day/night changes resume automatic operation
    // only when enabled rain protection is inactive.
    if (!(autoRainEnabled && rainActive) && autoDayNightEnabled) {
      manualOverrideActive = false;
      desiredOpen = autoDay;
    }

    Serial.println(
      autoDay ? "Automatic mode: DAY"
              : "Automatic mode: NIGHT"
    );
  }

  // ---------- AUTOMATIC TARGET ----------
  if (autoDayNightEnabled &&
      !manualOverrideActive &&
      !(autoRainEnabled && rainActive)) {
    desiredOpen = autoDay;
  }

  const bool rainProtectionActive = autoRainEnabled && rainActive;

  // ---------- MANUAL BUTTON ----------
  const bool rawButton = digitalRead(BUTTON_PIN);

  if (rawButton != lastRawButton) {
    lastRawButton = rawButton;
    lastButtonChangeAt = now;
  }

  if ((uint32_t)(now - lastButtonChangeAt) >=
          BUTTON_DEBOUNCE_MS &&
      rawButton != stableButton) {

    stableButton = rawButton;

    // Detect a new press.
    if (stableButton == HIGH) {

      if (rainProtectionActive) {
        // Do not change the saved restore state during rain.
        desiredOpen = false;

        Serial.println(
          "Button ignored: rain protection is active."
        );

      } else if (!motorRunning) {
        desiredOpen =
            (windowState == WindowState::Closed);

        manualOverrideActive = true;

        Serial.println(
          desiredOpen
            ? "Button: MANUAL OPEN"
            : "Button: MANUAL CLOSE"
        );

      } else {
        Serial.println(
          "Button ignored: motor is moving."
        );
      }
    }
  }

  // ---------- RAIN PRIORITY ----------
  if (rainProtectionActive) {
    desiredOpen = false;

    // Stop an opening motor immediately.
    // The next control step will command closing.
    if (motorRunning && movingTowardOpen) {
      stopMotor();

      Serial.println(
        "RAIN: opening stopped; closing prioritized."
      );
    }
  }

  // ---------- START REQUIRED MOVEMENT ----------
  if (!motorRunning &&
      desiredOpen != (windowState == WindowState::Open)) {

    startMotion(desiredOpen);
  }

  // ---------- MOTOR TIMEOUT ----------
  if (motorRunning &&
      (uint32_t)(now - motorStartedAt) >= MOTOR_RUN_MS) {

    const bool completedOpening = movingTowardOpen;

    stopMotor();

    windowState = completedOpening
                    ? WindowState::Open
                    : WindowState::Closed;

    Serial.println(
      completedOpening
        ? "Window assumed OPEN (timer elapsed)."
        : "Window assumed CLOSED (timer elapsed)."
    );
  }

  // ---------- WI-FI API ----------
  if (WiFi.status() == WL_CONNECTED) {
    if (!routerConnectionReported) {
      Serial.print("Router Wi-Fi connected. ESP32 address: http://");
      Serial.println(WiFi.localIP());
      routerConnectionReported = true;
    }
  } else {
    routerConnectionReported = false;
    if ((uint32_t)(now - lastWiFiRetryAt) >= 10000) {
      lastWiFiRetryAt = now;
      WiFi.reconnect();
    }
  }

  if (webServerStarted) {
    server.handleClient();
  }

  // ---------- SERIAL MONITOR DEBUG ----------
  static uint32_t lastReportAt = 0;

  if ((uint32_t)(now - lastReportAt) >= 1000) {
    lastReportAt = now;

    Serial.print("Light ADC: ");
    Serial.print(lightLevel);

    Serial.print(" | Rain raw pin: ");
    Serial.print(digitalRead(RAIN_PIN));

    Serial.print(" | Rain status: ");
    Serial.print(rainActive ? "WET" : "DRY");

    Serial.print(" | Auto mode: ");
    Serial.print(autoDay ? "DAY" : "NIGHT");

    Serial.print(" | Target: ");
    Serial.print(desiredOpen ? "OPEN" : "CLOSED");

    Serial.print(" | Motor: ");
    Serial.print(motorRunning ? "RUNNING" : "STOPPED");

    Serial.print(" | Manual override: ");
    Serial.println(
      manualOverrideActive ? "YES" : "NO"
    );
  }
}
