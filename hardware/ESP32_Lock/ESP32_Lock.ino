#include <Adafruit_Fingerprint.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>
#include <HTTPClient.h>
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <Preferences.h>
#include <WiFi.h>

// ------------------------- Configuration -------------------------
const char *ssid = "Room_6_7";
const char *password = "Suraj@123";
const char *serverPollUrl = "http://192.168.0.112:8000/api/device/poll"; 
const char *serverResultUrl = "http://192.168.0.112:8000/api/device/result"; 

#define SERVO_PIN 13 
#define FINGER_RX 16 
#define FINGER_TX 17 

// ------------------------- Globals -------------------------
HardwareSerial mySerial(2);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);
LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo doorServo;
Preferences prefs;

// --- Keypad ---
const byte ROWS = 4, COLS = 3;
char keys[ROWS][COLS] = {
    {'1', '2', '3'}, 
    {'4', '5', '6'}, 
    {'7', '8', '9'}, 
    {'*', '0', '#'}};
byte rowPins[ROWS] = {32, 33, 25, 26}; 
byte colPins[COLS] = {27, 14, 12}; 
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ------------------------- State & Timers -------------------------
enum SystemState {
  LOCKED,
  UNLOCKED,
  AWAITING_ADMIN_PIN,
  ADMIN_MENU,
  ENROLL_WAIT_FIRST,
  ENROLL_WAIT_REMOVE,
  ENROLL_WAIT_SECOND
};

SystemState currentState = LOCKED;

String inputPin = "";
String adminPin = "1234";
String userPin = "5678";

int enrollTargetId = -1;
bool isOfflineEnrollment = false;

unsigned long stateTimer = 0;
unsigned long temporaryMessageTimer = 0;
unsigned long unlockTimer = 0;
unsigned long lastHttpPoll = 0;
unsigned long lastFingerprintScan = 0;
unsigned long lastWiFiReconnect = 0; 

#define HTTP_POLL_INTERVAL 2000
#define ENROLL_TIMEOUT 30000
#define AUTH_TIMEOUT 15000
#define FINGER_POLL_INTERVAL 100 
#define WIFI_RECONNECT_INTERVAL 10000 

bool isMessageTemporary = false;
bool wasConnected = false; 

// ------------------------- Display Helpers -------------------------

// Helper function to prevent LCD flickering. 
// Pads strings to 16 chars so it overwrites old text without needing lcd.clear()
void printRow(int row, String text) {
  lcd.setCursor(0, row);
  while (text.length() < 16) {
    text += " ";
  }
  lcd.print(text);
}

// Helper to mask PINs for security (shows *** instead of numbers)
String getMaskedPin(String pin) {
  String masked = "";
  for(int i = 0; i < pin.length(); i++) masked += "*";
  return masked;
}

void updateDisplay() {
  if (isMessageTemporary) return;

  switch (currentState) {
  case LOCKED:
    if (WiFi.status() == WL_CONNECTED) {
      printRow(0, "Locked (Online)");
    } else {
      printRow(0, "Locked (Offline)");
    }
    
    if (inputPin.length() > 0) {
      printRow(1, "PIN: " + getMaskedPin(inputPin));
    } else {
      printRow(1, "Scan Finger/PIN");
    }
    break;

  case UNLOCKED:
    printRow(0, "Door Unlocked");
    printRow(1, "");
    break;

  case AWAITING_ADMIN_PIN:
    printRow(0, "Admin ID 1-10");
    printRow(1, inputPin.length() > 0 ? "PIN: " + getMaskedPin(inputPin) : "PIN + #:");
    break;

  case ADMIN_MENU:
    printRow(0, "1:Unlck 2:Add");
    printRow(1, "3:Wipe *:Cancel");
    break;

  case ENROLL_WAIT_FIRST:
    printRow(0, isOfflineEnrollment ? "Offline Enroll" : "Enroll Mode");
    printRow(1, "Place finger...");
    break;

  case ENROLL_WAIT_REMOVE:
    printRow(0, "Remove finger");
    printRow(1, "");
    break;

  case ENROLL_WAIT_SECOND:
    printRow(0, "Place same");
    printRow(1, "finger again...");
    break;
  }
}

void showTemporaryMessage(String l1, String l2, unsigned long durationMs) {
  printRow(0, l1);
  printRow(1, l2);
  temporaryMessageTimer = millis() + durationMs;
  isMessageTemporary = true;
}

void resetToLocked() {
  currentState = LOCKED;
  inputPin = "";
  enrollTargetId = -1;
  isOfflineEnrollment = false;
  isMessageTemporary = false;
  doorServo.write(0);
  updateDisplay();
}

void unlockDoor() {
  currentState = UNLOCKED;
  doorServo.write(90);
  unlockTimer = millis() + 3000;
  showTemporaryMessage("Access Granted!", "Unlocking...", 3000);
}

// ------------------------- HTTP Comms & WiFi -------------------------
void handleWiFi() {
  bool isConnected = (WiFi.status() == WL_CONNECTED);
  
  if (isConnected != wasConnected) {
    wasConnected = isConnected;
    if (currentState == LOCKED && !isMessageTemporary) {
      updateDisplay();
    }
  }

  if (!isConnected && (millis() - lastWiFiReconnect >= WIFI_RECONNECT_INTERVAL)) {
    Serial.println("[WiFi] Connection lost. Attempting reconnect...");
    WiFi.reconnect();
    lastWiFiReconnect = millis();
  }
}

void sendEnrollResult(int id, bool success) {
  if (isOfflineEnrollment) {
    if (success) {
      int nextId = prefs.getInt("off_target", 11);
      prefs.putInt("off_target", nextId + 1);
      showTemporaryMessage("ID " + String(id), "Saved Offline!", 2500);
    }
    return; 
  }

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverResultUrl);
    http.addHeader("Content-Type", "application/json");
    String requestBody =
        "{\"action\":\"ENROLL_RESULT\",\"hardwareId\":" + String(id) +
        ",\"success\":" + (success ? "true" : "false") + "}";
    http.POST(requestBody);
    http.end();
    Serial.println("[ENROLL] Posted result: " + requestBody);
  }
}

void pollServer() {
  if (millis() - lastHttpPoll >= HTTP_POLL_INTERVAL &&
      WiFi.status() == WL_CONNECTED) {
    lastHttpPoll = millis();
    HTTPClient http;
    http.begin(serverPollUrl);
    int httpResponseCode = http.GET();

    // Ensure we only process if the server actually returned an OK (200) status
    if (httpResponseCode == HTTP_CODE_OK) {
      String payload = http.getString();
      JsonDocument doc;
      deserializeJson(doc, payload);

      String cmd = doc["command"].as<String>();
      if (cmd == "UPDATE_ADMIN_PIN") {
        adminPin = doc["pin"].as<String>();
        prefs.putString("adminPin", adminPin);
        showTemporaryMessage("Config Updated", "Admin PIN Synced!", 2500);
      } else if (cmd == "UPDATE_USER_PIN") {
        userPin = doc["pin"].as<String>();
        prefs.putString("userPin", userPin);
        showTemporaryMessage("Config Updated", "User PIN Synced!", 2500);
      } else if (cmd == "DELETE") {
        int delId = doc["hardwareId"].as<int>();
        finger.deleteModel(delId);
        showTemporaryMessage("ID Deleted", "ID: " + String(delId), 2500);
      } else if (cmd == "ENROLL" && currentState == LOCKED) {
        enrollTargetId = doc["hardwareId"].as<int>();
        isOfflineEnrollment = false;
        currentState = ENROLL_WAIT_FIRST;
        stateTimer = millis();
        isMessageTemporary = false;
        updateDisplay();
        Serial.println("[ENROLL] Waiting for first finger, ID=" + String(enrollTargetId));
      }
    }
    http.end();
  }
}

// ------------------------- Hardware Logic -------------------------
void handleEnrollState() {
  if (currentState != ENROLL_WAIT_FIRST && currentState != ENROLL_WAIT_REMOVE &&
      currentState != ENROLL_WAIT_SECOND) {
    return;
  }

  uint8_t p;

  if (currentState == ENROLL_WAIT_FIRST) {
    if (millis() - stateTimer > ENROLL_TIMEOUT) {
      showTemporaryMessage("Enroll Timeout", "Try again", 2500);
      sendEnrollResult(enrollTargetId, false);
      resetToLocked();
      return;
    }
    p = finger.getImage();
    if (p != FINGERPRINT_OK) return;

    p = finger.image2Tz(1);
    if (p != FINGERPRINT_OK) {
      showTemporaryMessage("Bad image", "Try again", 2000);
      return;
    }

    currentState = ENROLL_WAIT_REMOVE;
    isMessageTemporary = false;
    updateDisplay();
    stateTimer = millis();
    return;
  }

  if (currentState == ENROLL_WAIT_REMOVE) {
    p = finger.getImage();
    if (p == FINGERPRINT_NOFINGER) {
      currentState = ENROLL_WAIT_SECOND;
      isMessageTemporary = false;
      updateDisplay();
      stateTimer = millis();
    }
    return;
  }

  if (currentState == ENROLL_WAIT_SECOND) {
    if (millis() - stateTimer > ENROLL_TIMEOUT) {
      showTemporaryMessage("Enroll Timeout", "Try again", 2500);
      sendEnrollResult(enrollTargetId, false);
      resetToLocked();
      return;
    }
    p = finger.getImage();
    if (p != FINGERPRINT_OK) return;

    p = finger.image2Tz(2);
    if (p != FINGERPRINT_OK) {
      showTemporaryMessage("Bad image", "Place again...", 2000);
      return;
    }

    p = finger.createModel();
    if (p != FINGERPRINT_OK) {
      showTemporaryMessage("Fingers differ!", "Enroll failed", 2500);
      sendEnrollResult(enrollTargetId, false);
      resetToLocked();
      return;
    }

    p = finger.storeModel(enrollTargetId);
    if (p == FINGERPRINT_OK) {
      showTemporaryMessage("Enrolled!", "ID: " + String(enrollTargetId), 3000);
      sendEnrollResult(enrollTargetId, true);
    } else {
      showTemporaryMessage("Store failed", "ID: " + String(enrollTargetId), 2500);
      sendEnrollResult(enrollTargetId, false);
    }
    resetToLocked();
  }
}

void checkFingerprint() {
  if (currentState != LOCKED) return;

  if (millis() - lastFingerprintScan < FINGER_POLL_INTERVAL) return;
  lastFingerprintScan = millis();

  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK) return;

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) return;

  p = finger.fingerFastSearch();
  if (p == FINGERPRINT_OK) {
    if (finger.fingerID >= 1 && finger.fingerID <= 10) {
      currentState = AWAITING_ADMIN_PIN;
      inputPin = "";
      stateTimer = millis();
      isMessageTemporary = false;
      updateDisplay();
    } else {
      unlockDoor();
    }
  } else {
    showTemporaryMessage("Access Denied", "No Match!", 2500);
  }
}

void handleKeypad() {
  char key = keypad.getKey();

  if (currentState == AWAITING_ADMIN_PIN &&
      millis() - stateTimer > AUTH_TIMEOUT) {
    showTemporaryMessage("Auth Timeout", "Returning...", 2000);
    resetToLocked();
    return;
  }

  if (!key) return;
  stateTimer = millis();

  if (currentState == LOCKED) {
    if (key == '#') {
      if (inputPin == adminPin || inputPin == userPin) {
        unlockDoor();
      } else {
        showTemporaryMessage("Wrong PIN", "", 2500);
      }
      inputPin = "";
    } else if (key == '*') {
      inputPin = "";
      showTemporaryMessage("Cleared", "", 1500);
    } else {
      inputPin += key;
      isMessageTemporary = false;
      updateDisplay();
    }
  } else if (currentState == AWAITING_ADMIN_PIN) {
    if (key == '#') {
      if (inputPin == adminPin) {
        currentState = ADMIN_MENU;
        isMessageTemporary = false;
        updateDisplay();
      } else {
        showTemporaryMessage("Wrong PIN", "", 2500);
        resetToLocked();
      }
      inputPin = "";
    } else if (key == '*') {
      resetToLocked();
    } else {
      inputPin += key;
      isMessageTemporary = false;
      updateDisplay();
    }
  } else if (currentState == ADMIN_MENU) {
    if (key == '1') {
      unlockDoor();
    } else if (key == '2') {
      int target = prefs.getInt("off_target", 11);
      enrollTargetId = target;
      isOfflineEnrollment = true;
      currentState = ENROLL_WAIT_FIRST;
      stateTimer = millis();
      isMessageTemporary = false;
      updateDisplay();
    } else if (key == '3') {
      finger.emptyDatabase();
      prefs.putInt("off_target", 11);
      showTemporaryMessage("WIPE SUCCESS", "All IDs Cleared", 3500);
      resetToLocked();
    } else if (key == '*') {
      showTemporaryMessage("Exiting...", "", 1500);
      resetToLocked();
    }
  }
}

void handleTimers() {
  if (isMessageTemporary && millis() > temporaryMessageTimer) {
    isMessageTemporary = false;
    // Calling updateDisplay automatically repaints the correct state over the temp message
    updateDisplay(); 
  }

  if (currentState == UNLOCKED && millis() > unlockTimer) {
    resetToLocked();
  }
}

// ------------------------- Setup & Loop -------------------------
void setup() {
  Serial.begin(115200);

  doorServo.setPeriodHertz(50);
  doorServo.attach(SERVO_PIN, 500, 2400); 
  doorServo.write(0);

  lcd.init();
  lcd.backlight();
  printRow(0, "Booting...");

  mySerial.begin(57600, SERIAL_8N1, FINGER_RX, FINGER_TX);
  if (!finger.verifyPassword()) {
    mySerial.begin(9600, SERIAL_8N1, FINGER_RX, FINGER_TX); 
  }

  prefs.begin("proxigate", false);
  adminPin = prefs.getString("adminPin", adminPin); 
  userPin = prefs.getString("userPin", userPin); 

  WiFi.begin(ssid, password);
  Serial.print("[WiFi] Connecting");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 10) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println("");
  
  wasConnected = (WiFi.status() == WL_CONNECTED);

  resetToLocked();
}

void loop() {
  handleWiFi(); 

  if (currentState == LOCKED && millis() - stateTimer > 2000) {
    pollServer();
  }

  handleTimers();
  handleEnrollState();
  checkFingerprint();
  handleKeypad();
}