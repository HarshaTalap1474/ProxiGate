#include "Arduino.h"
#include "esp_camera.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#include "fb_gfx.h"
#include "img_converters.h"
#include "soc/rtc_cntl_reg.h" // disable brownout problems
#include "soc/soc.h"          // disable brownout problems
#include <HTTPClient.h>
#include <WiFi.h>

#include <Preferences.h>
#include <WiFiManager.h>
#include "esp_wifi.h"

char serverIp[40] = "192.168.0.112";

#define PART_BOUNDARY "123456789000000000000987654321"

// OV2640 CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

static const char *_STREAM_CONTENT_TYPE =
    "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *_STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char *_STREAM_PART =
    "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

httpd_handle_t stream_httpd = NULL;

static esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t *fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t *_jpg_buf = NULL;
  char *part_buf[64];

  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if (res != ESP_OK)
    return res;

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      res = ESP_FAIL;
    } else {
      _jpg_buf_len = fb->len;
      _jpg_buf = fb->buf;
    }
    if (res == ESP_OK) {
      size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
      res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY,
                                  strlen(_STREAM_BOUNDARY));
    }
    if (fb) {
      esp_camera_fb_return(fb);
      fb = NULL;
      _jpg_buf = NULL;
    } else if (_jpg_buf) {
      free(_jpg_buf);
      _jpg_buf = NULL;
    }
    if (res != ESP_OK) {
      break;
    }
  }
  return res;
}

void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 81;
  config.core_id = 1; // CRITICAL FIX: Pin stream server to Core 1 so it doesn't fight the Wi-Fi radio on Core 0

  httpd_uri_t stream_uri = {.uri = "/stream",
                            .method = HTTP_GET,
                            .handler = stream_handler,
                            .user_ctx = NULL};

  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &stream_uri);
  }
}

void notifyServerOfIP(String ipStr) {
  HTTPClient http;
  String fullUrl = "http://" + String(serverIp) + ":8000/api/device/cam_ip?ip=" + ipStr;
  
  http.setReuse(false);     // Disable connection keep-alive pooling to prevent socket exhaustion
  http.setTimeout(15000);   // Massive 15 second timeout to penetrate congested WiFi buffers during stream
  http.begin(fullUrl);
  
  int httpCode = http.GET(); 
  if (httpCode > 0) {
    Serial.println("Registered IP with Backend: " + ipStr);
  } else {
    Serial.println("Failed to register IP with Backend. URL: " + fullUrl + " Error: " + http.errorToString(httpCode));
  }
  http.end();
}

#define WIFI_RESET_PIN 13 // Hold GPIO 13 to GND for 3 seconds anytime to wipe WiFi

bool shouldSaveConfigCam = false;
void saveConfigCallbackCam () {
  shouldSaveConfigCam = true;
}

// RTOS Task for Background Heartbeat Ping
void heartbeatTaskLoop(void * pvParameters) {
  for (;;) {
    vTaskDelay(30000 / portTICK_PERIOD_MS); // Dialed back to 30 seconds to prevent starving the MJPEG bandwidth
    if (WiFi.status() == WL_CONNECTED) {
      notifyServerOfIP(WiFi.localIP().toString());
    }
  }
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // Disable brownout detector

  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();
  
  // Initialize the reset pin
  pinMode(WIFI_RESET_PIN, INPUT_PULLUP);

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_VGA; 
    config.jpeg_quality = 14; // Increase compression slightly (10 -> 14). This cuts file size by ~30%, allowing the Wi-Fi to push 24+ fps smoothly without dropping frames.
    config.fb_count = 2;
    config.grab_mode = CAMERA_GRAB_LATEST; // CRITICAL: This drops old frames instead of lagging
  } else {
    config.frame_size = FRAMESIZE_SVGA; 
    config.fb_location = CAMERA_FB_IN_DRAM;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }
  
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
  
  // Rotate the camera stream 180 degrees immediately after INIT
  sensor_t * s = esp_camera_sensor_get();
  if (s != NULL) {
    delay(100); // Give the SCCB bus a moment
    s->set_vflip(s, 1);   // flip vertically
    delay(100);
    s->set_hmirror(s, 1); // mirror horizontally
    delay(100);
  }

  // Decrease WiFi transmit power just in case
  WiFi.mode(WIFI_STA); 
  WiFi.disconnect();
  delay(100);
  WiFi.setTxPower(WIFI_POWER_11dBm);

  // Connect to WiFi via WiFiManager
  Preferences prefs;
  prefs.begin("proxigate", false);
  String savedIp = prefs.getString("serverIp", "192.168.0.112");
  strcpy(serverIp, savedIp.c_str());

  WiFiManagerParameter custom_server_ip("serverIp", "Backend IP", serverIp, 40);
  WiFiManager wm;
  wm.setSaveConfigCallback(saveConfigCallbackCam);
  wm.addParameter(&custom_server_ip);

  Serial.println("Connecting to WiFi via Captive Portal...");
  if (!wm.autoConnect("ProxiGate-Cam")) {
    Serial.println("Failed to connect. Restarting...");
    delay(3000);
    ESP.restart();
  }
  
  // CRITICAL FIX: WiFiManager often leaves the chip in WIFI_AP_STA mode. 
  // The AP beacon interrupts the WiFi radio drastically, causing severe MJPEG lag.
  // We strictly force it back to pure Station mode here:
  WiFi.mode(WIFI_STA); 

  if (shouldSaveConfigCam) {
    strcpy(serverIp, custom_server_ip.getValue());
    prefs.putString("serverIp", serverIp);
  }

  Serial.println("");
  Serial.println("WiFi connected");

  Serial.print("Camera Stream Ready! Go to: http://");
  String localIp = WiFi.localIP().toString();
  Serial.print(localIp);
  Serial.println(":81/stream");

  startCameraServer();
  notifyServerOfIP(localIp); // Tell backend our IP so dashboard auto updates
  
  // Disable WiFi Power Save Mode immediately (Crucial for lag-free ESP32-CAM streams)
  esp_wifi_set_ps(WIFI_PS_NONE);

  // Spawn the Heartbeat task precisely onto Core 0 (App Core) to separate it from the camera stream processes
  xTaskCreatePinnedToCore(
    heartbeatTaskLoop,
    "HeartbeatTask",
    4096,
    NULL,
    1,
    NULL,
    0
  );
}

void loop() {
  static int holdTime = 0;

  // Check if reset button is pressed
  if (digitalRead(WIFI_RESET_PIN) == LOW) {
    holdTime += 1000;
    
    if (holdTime >= 3000) {
      Serial.println("\n[RESET] Pin 13 held for 3 seconds! Erasing saved networks...");
      
      // 1. Wipe WiFi credentials
      WiFiManager wm;
      wm.resetSettings();
      
      // 2. Wipe the custom backend server IP
      Preferences prefs;
      prefs.begin("proxigate", false);
      prefs.clear();
      prefs.end();

      Serial.println("[RESET] Device completely wiped. Restarting in 2 seconds...");
      delay(2000);
      ESP.restart(); // Reboot so the Captive Portal comes back up
    }
  } else {
    holdTime = 0; // Instantly reset the hold timer if released
  }

  // Restore the original 1000ms delay that produced zero lag.
  // This gives maximum uninterrupted FreeRTOS time to the MJPEG streaming task!
  delay(1000);
}
