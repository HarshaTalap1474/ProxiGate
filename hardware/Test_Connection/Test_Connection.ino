#include <WiFi.h>
#include <HTTPClient.h>

const char *ssid = "Room_6_7";
const char *password = "Suraj@123";
const char *serverPollUrl = "http://192.168.0.106:8000/api/device/poll"; 

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n--- ESP32 Connection Debugger ---");

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n[WiFi] Connected!");
  Serial.print("ESP32 IP Address: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[HTTP] Sending GET request to backend...");
    unsigned long start_time = millis();
    
    HTTPClient http;
    // Set a moderate timeout (approx 3 seconds) instead of the default long timeout
    http.setTimeout(3000); 
    http.begin(serverPollUrl);
    
    int httpResponseCode = http.GET();
    unsigned long duration = millis() - start_time;

    Serial.print("[HTTP] Duration: ");
    Serial.print(duration);
    Serial.println(" ms");

    if (httpResponseCode > 0) {
      Serial.print("[HTTP] Success! Response Code: ");
      Serial.println(httpResponseCode);
      String payload = http.getString();
      Serial.print("[HTTP] Payload: ");
      Serial.println(payload);
    } else {
      Serial.print("[HTTP] FAILING. Error code: ");
      Serial.println(httpResponseCode);
      Serial.println(http.errorToString(httpResponseCode).c_str());
      Serial.println("-> TIP: If this times out (-1), Windows Defender Firewall is blocking port 8000 on your PC!");
    }
    
    http.end();
  } else {
    Serial.println("WiFi Disconnected...");
  }

  // Wait 4 seconds before next ping
  delay(4000);
}
