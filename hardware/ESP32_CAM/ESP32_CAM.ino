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
  http.begin(fullUrl);
  int httpCode = http.GET(); 
  if (httpCode > 0) {
    Serial.println("Registered IP with Backend: " + ipStr);
  } else {
    Serial.println("Failed to register IP with Backend. URL: " + fullUrl + " Error: " + http.errorToString(httpCode));
  }
  http.end();
}

#define WIFI_RESET_PIN 13 // Connect GPIO 13 to GND during boot to wipe WiFi

bool shouldSaveConfigCam = false;
void saveConfigCallbackCam () {
  shouldSaveConfigCam = true;
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // Disable brownout detector

  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();
  
  // Check if user wants to reset WiFi credentials
  pinMode(WIFI_RESET_PIN, INPUT_PULLUP);
  if (digitalRead(WIFI_RESET_PIN) == LOW) {
    Serial.println("WiFi Reset Pin activated! Erasing saved networks...");
    WiFiManager wm;
    wm.resetSettings();
    delay(1000);
    Serial.println("WiFi Data Erased. Starting normal boot into Captive Portal...");
  }

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

  if (psramFound()) {
    config.frame_size = FRAMESIZE_QVGA; 
    config.jpeg_quality = 20; 
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_QVGA; 
    config.jpeg_quality = 25;
    config.fb_count = 1;
  }
  
  // Decrease WiFi transmit power to prevent 500mA USB power spikes which cause the "wifi:Set status to INIT" panic!
  WiFi.mode(WIFI_STA); 
  WiFi.disconnect();
  delay(100);
  WiFi.setTxPower(WIFI_POWER_11dBm); // Lowers max power spike from ~700mA down to ~300mA

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
  
  if (shouldSaveConfigCam) {
    strcpy(serverIp, custom_server_ip.getValue());
    prefs.putString("serverIp", serverIp);
  }

  Serial.println("");
  Serial.println("WiFi connected");
  
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
  
  // Rotate the camera stream 180 degrees
  sensor_t * s = esp_camera_sensor_get();
  if (s != NULL) {
    s->set_vflip(s, 1);   // flip vertically
    s->set_hmirror(s, 1); // mirror horizontally
  }

  Serial.print("Camera Stream Ready! Go to: http://");
  String localIp = WiFi.localIP().toString();
  Serial.print(localIp);
  Serial.println(":81/stream");

  startCameraServer();
  notifyServerOfIP(localIp); // Tell backend our IP so dashboard auto updates
}

void loop() {
  delay(10000);
  if (WiFi.status() == WL_CONNECTED) {
    notifyServerOfIP(WiFi.localIP().toString());
  }
}
