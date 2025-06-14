#include "esp_camera.h"
#include <WiFi.h>
#include <esp_http_server.h>
#include <HardwareSerial.h>

// WiFi credentials
const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";

// GPS UART config
HardwareSerial GPS_Serial(2);  // UART2
#define GPS_RX 16              // Connect to TX of NEO-6M
#define GPS_TX 17              // Not used

// Camera pin config for AI Thinker
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

httpd_handle_t camera_httpd = NULL;

// GPS parser (simple, extracts $GPGGA)
String readGPSData() {
  String gpsData = "";
  while (GPS_Serial.available()) {
    char c = GPS_Serial.read();
    gpsData += c;
    if (c == '\n') break;
  }
  if (gpsData.startsWith("$GPGGA")) {
    return gpsData;
  }
  return "";
}

String parseLatitude(String gpgga) {
  int idx1 = gpgga.indexOf(',') + 1;
  int idx2 = gpgga.indexOf(',', idx1);
  int idx3 = gpgga.indexOf(',', idx2 + 1);
  String lat = gpgga.substring(idx1, idx2);
  String ns = gpgga.substring(idx2 + 1, idx3);
  return lat + " " + ns;
}

String parseLongitude(String gpgga) {
  int idx = 0;
  for (int i = 0; i < 4; i++) idx = gpgga.indexOf(',', idx + 1);
  int idx2 = gpgga.indexOf(',', idx + 1);
  int idx3 = gpgga.indexOf(',', idx2 + 1);
  String lon = gpgga.substring(idx + 1, idx2);
  String ew = gpgga.substring(idx2 + 1, idx3);
  return lon + " " + ew;
}

// GPS endpoint handler
esp_err_t gps_handler(httpd_req_t *req) {
  String gpgga;
  for (int i = 0; i < 10; i++) {
    gpgga = readGPSData();
    if (gpgga.length() > 0) break;
    delay(200);
  }

  String json;
  if (gpgga.length() > 0) {
    String lat = parseLatitude(gpgga);
    String lon = parseLongitude(gpgga);
    json = "{\"latitude\": \"" + lat + "\", \"longitude\": \"" + lon + "\"}";
  } else {
    json = "{\"error\": \"No GPS fix\"}";
  }

  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, json.c_str(), json.length());
  return ESP_OK;
}

// Camera stream handler 
esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len;
  uint8_t * _jpg_buf;
  char part_buf[64];

  static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=frame";
  static const char* _STREAM_BOUNDARY = "\r\n--frame\r\n";
  static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if (res != ESP_OK) return res;

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      continue;
    }

    _jpg_buf_len = fb->len;
    _jpg_buf = fb->buf;
    snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
    res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    res = httpd_resp_send_chunk(req, (const char *)part_buf, strlen((char *)part_buf));
    res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    esp_camera_fb_return(fb);
    if (res != ESP_OK) break;
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }

  return res;
}

void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
    httpd_uri_t stream_uri = {
      .uri       = "/stream",
      .method    = HTTP_GET,
      .handler   = stream_handler,
      .user_ctx  = NULL
    };
    httpd_register_uri_handler(camera_httpd, &stream_uri);

    httpd_uri_t gps_uri = {
      .uri       = "/gps",
      .method    = HTTP_GET,
      .handler   = gps_handler,
      .user_ctx  = NULL
    };
    httpd_register_uri_handler(camera_httpd, &gps_uri);
  }
}

void setup() {
  Serial.begin(115200);
  GPS_Serial.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected.");
  Serial.print("Visit http://");
  Serial.print(WiFi.localIP());
  Serial.println("/stream or /gps");

  // Camera configuration
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
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_QVGA;
  config.jpeg_quality = 10;
  config.fb_count = 2;

  // Camera init
  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Camera init failed");
    return;
  }

  startCameraServer();
}

void loop() {
  // Nothing to do here
}
