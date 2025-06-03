#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include "esp_http_server.h"
#include "mbedtls/base64.h"

// Kamera modeli AI Thinker
#define CAMERA_MODEL_AI_THINKER
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

// AI modeli URL'si
const char* post_url = "https://handleimage-tyhvqqi4xa-uc.a.run.app";

// SoftAP bilgileri
const char *ap_ssid = "ESP32-CAM-Setup";
const char *ap_password = "12345678";

// Wi-Fi bilgileri
String wifi_ssid = "";
String wifi_password = "";

// SoftAP web sunucusu
WebServer setupServer(80);

// Kamera HTTP sunucusu
httpd_handle_t camera_httpd = NULL;

bool wifiConnected = false;

// Fotoğraf çekme ve POST etme işlevi
esp_err_t capture_handler(httpd_req_t *req) {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  size_t output_length;
  size_t base64_len = (4 * ((fb->len + 2) / 3)) + 1;
  uint8_t *base64_buf = (uint8_t *)malloc(base64_len);
  if (!base64_buf) {
    esp_camera_fb_return(fb);
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  int ret = mbedtls_base64_encode(base64_buf, base64_len, &output_length, fb->buf, fb->len);
  esp_camera_fb_return(fb);
  if (ret != 0) {
    free(base64_buf);
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  HTTPClient http;
  http.begin(post_url);
  http.addHeader("Content-Type", "application/json");
  String jsonBody = "{\"image\": \"" + String((char *)base64_buf) + "\"}";
  int httpResponseCode = http.POST(jsonBody);

  http.end();
  free(base64_buf);

  httpd_resp_set_type(req, "text/plain");
  httpd_resp_send(req, httpResponseCode > 0 ? "Fotoğraf gönderildi." : "POST başarısız!", HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

// Kamera akışı işlevi
esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t *fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t *_jpg_buf = NULL;
  char *part_buf[64];

  res = httpd_resp_set_type(req, "multipart/x-mixed-replace; boundary=frame");
  if (res != ESP_OK) return res;

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      res = ESP_FAIL;
      break;
    }
    if (fb->format != PIXFORMAT_JPEG) {
      bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
      esp_camera_fb_return(fb);
      if (!jpeg_converted) {
        res = ESP_FAIL;
        break;
      }
    } else {
      _jpg_buf_len = fb->len;
      _jpg_buf = fb->buf;
    }

    if (res == ESP_OK) {
      size_t hlen = snprintf((char *)part_buf, 64,
                             "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", _jpg_buf_len);
      res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
    }

    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    }

    if (fb->format == PIXFORMAT_JPEG) {
      esp_camera_fb_return(fb);
    } else if (_jpg_buf) {
      free(_jpg_buf);
    }

    if (res != ESP_OK) break;
    delay(10);
  }
  return res;
}

esp_err_t index_handler(httpd_req_t *req) {
  const char *resp_str = "ESP32-CAM: <a href='/capture'>Fotoğraf Çek</a> | <a href='/stream'>Canlı Yayın</a>";
  return httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);
}

void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
    httpd_uri_t uri_index = {"/", HTTP_GET, index_handler, NULL};
    httpd_uri_t uri_capture = {"/capture", HTTP_GET, capture_handler, NULL};
    httpd_uri_t uri_stream = {"/stream", HTTP_GET, stream_handler, NULL};
    httpd_register_uri_handler(camera_httpd, &uri_index);
    httpd_register_uri_handler(camera_httpd, &uri_capture);
    httpd_register_uri_handler(camera_httpd, &uri_stream);
    Serial.println("Kamera sunucusu başlatıldı.");
  }
}

// Kamera ayarları
void initCamera() {
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

  config.frame_size = FRAMESIZE_QVGA;  // 320x240 çözünürlük
  config.jpeg_quality = 12;            // Orta kalite (0-63 arası)
  config.fb_count = 2;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Kamera başlatılamadı! Hata kodu: 0x%x\n", err);
  }
}

// Wi-Fi bağlantısı sonrası işlemler
void tryConnectAndStartCamera() {
  WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
  Serial.print("WiFi'ye bağlanılıyor");
  int timeout = 20;
  while (WiFi.status() != WL_CONNECTED && timeout-- > 0) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi bağlantısı başarılı!");
    Serial.println(WiFi.localIP());
    initCamera();
    startCameraServer();
    wifiConnected = true;
  } else {
    Serial.println("\nBağlantı başarısız!");
  }
}

void setupSoftAP() {
  WiFi.softAP(ap_ssid, ap_password);
  Serial.println("SoftAP açıldı.");
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

setupServer.on("/", HTTP_GET, []() {
  setupServer.send(200, "text/html",
    "<!DOCTYPE html>"
    "<html>"
    "<head>"
    "<meta charset='UTF-8'>"
    "<title>WiFi Configuration</title>"
    "<style>"
    "body {"
    "  display: flex;"
    "  justify-content: center;"
    "  align-items: center;"
    "  height: 100vh;"
    "  margin: 0;"
    "  background-color: #f0f0f0;"
    "  font-family: Arial, sans-serif;"
    "}"
    "form {"
    "  width: 320px;"
    "  padding: 20px;"
    "  box-sizing: border-box;"
    "  background: white;"
    "  border-radius: 10px;"
    "  box-shadow: 0 0 10px rgba(0, 0, 0, 0.1);"
    "}"
    "h3 {"
    "  margin-top: 0;"
    "  margin-bottom: 15px;"
    "  text-align: center;"
    "}"
    "input[type='text'], input[type='password'], input[type='submit'] {"
    "  box-sizing: border-box;"
    "  width: 100%;"
    "  padding: 10px;"
    "  margin-bottom: 15px;"
    "  border: 1px solid #ccc;"
    "  border-radius: 5px;"
    "}"
    "input[type='submit'] {"
    "  background-color: #007BFF;"
    "  color: white;"
    "  border: none;"
    "  cursor: pointer;"
    "}"
    "input[type='submit']:hover {"
    "  background-color: #0056b3;"
    "}"
    "</style>"
    "</head>"
    "<body>"
    "<form action='/connect' method='POST'>"
    "<h3>WiFi Settings</h3>"
    "<label>SSID:</label><input type='text' name='ssid' required>"
    "<label>Password:</label><input type='password' name='password' required>"
    "<input type='submit' value='Connect'>"
    "</form>"
    "</body>"
    "</html>");
});



  setupServer.on("/connect", HTTP_POST, []() {
    wifi_ssid = setupServer.arg("ssid");
    wifi_password = setupServer.arg("password");

    setupServer.send(200, "text/html", "<h3>Bağlanıyor...</h3>");
    setupServer.stop();  // Server'ı durduruyoruz
    WiFi.softAPdisconnect(true);  // SoftAP'yi doğru sırayla kapatıyoruz
    tryConnectAndStartCamera();
  });

  setupServer.begin();
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  setupSoftAP();
}

void loop() {
  if (!wifiConnected) {
    setupServer.handleClient();
  }
}






// 3 haziran
#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include "esp_http_server.h"
#include "mbedtls/base64.h"

// Kamera modeli AI Thinker
#define CAMERA_MODEL_AI_THINKER
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

// AI modeli URL'si
const char* post_url = "https://handleimage-tyhvqqi4xa-uc.a.run.app";

// SoftAP bilgileri
const char *ap_ssid = "ESP32-CAM-Setup";
const char *ap_password = "12345678";

// Wi-Fi bilgileri
String wifi_ssid = "";
String wifi_password = "";

// SoftAP web sunucusu
WebServer setupServer(80);

// Kamera HTTP sunucusu
httpd_handle_t camera_httpd = NULL;

bool wifiConnected = false;

// Fotoğraf çekme ve POST etme işlevi
esp_err_t capture_handler(httpd_req_t *req) {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  size_t output_length;
  size_t base64_len = (4 * ((fb->len + 2) / 3)) + 1;
  uint8_t *base64_buf = (uint8_t *)malloc(base64_len);
  if (!base64_buf) {
    esp_camera_fb_return(fb);
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  int ret = mbedtls_base64_encode(base64_buf, base64_len, &output_length, fb->buf, fb->len);
  esp_camera_fb_return(fb);
  if (ret != 0) {
    free(base64_buf);
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  HTTPClient http;
  http.begin(post_url);
  http.addHeader("Content-Type", "application/json");
  String jsonBody = "{\"image\": \"" + String((char *)base64_buf) + "\"}";
  int httpResponseCode = http.POST(jsonBody);

  http.end();
  free(base64_buf);

  httpd_resp_set_type(req, "text/plain");
  httpd_resp_send(req, httpResponseCode > 0 ? "Fotoğraf gönderildi." : "POST başarısız!", HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

// Kamera akışı işlevi
esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t *fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t *_jpg_buf = NULL;
  char *part_buf[64];

  res = httpd_resp_set_type(req, "multipart/x-mixed-replace; boundary=frame");
  if (res != ESP_OK) return res;

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      res = ESP_FAIL;
      break;
    }
    if (fb->format != PIXFORMAT_JPEG) {
      bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
      esp_camera_fb_return(fb);
      if (!jpeg_converted) {
        res = ESP_FAIL;
        break;
      }
    } else {
      _jpg_buf_len = fb->len;
      _jpg_buf = fb->buf;
    }

    if (res == ESP_OK) {
      size_t hlen = snprintf((char *)part_buf, 64,
                             "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", _jpg_buf_len);
      res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
    }

    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    }

    if (fb->format == PIXFORMAT_JPEG) {
      esp_camera_fb_return(fb);
    } else if (_jpg_buf) {
      free(_jpg_buf);
    }

    if (res != ESP_OK) break;
    delay(10);
  }
  return res;
}

esp_err_t index_handler(httpd_req_t *req) {
  const char *resp_str = "ESP32-CAM: <a href='/capture'>Fotoğraf Çek</a> | <a href='/stream'>Canlı Yayın</a>";
  return httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);
}

void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
    httpd_uri_t uri_index = {"/", HTTP_GET, index_handler, NULL};
    httpd_uri_t uri_capture = {"/capture", HTTP_GET, capture_handler, NULL};
    httpd_uri_t uri_stream = {"/stream", HTTP_GET, stream_handler, NULL};
    httpd_register_uri_handler(camera_httpd, &uri_index);
    httpd_register_uri_handler(camera_httpd, &uri_capture);
    httpd_register_uri_handler(camera_httpd, &uri_stream);
    Serial.println("Kamera sunucusu başlatıldı.");
  }
}

// Kamera ayarları
void initCamera() {
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

  config.frame_size = FRAMESIZE_QVGA;  // 320x240 çözünürlük
  config.jpeg_quality = 12;            // Orta kalite (0-63 arası)
  config.fb_count = 2;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Kamera başlatılamadı! Hata kodu: 0x%x\n", err);
  }
}

// Wi-Fi bağlantısı sonrası işlemler
void tryConnectAndStartCamera() {
  // Statik IP ayarları
  IPAddress local_IP(192, 168, 233, 100);  // SABİT IP
  IPAddress gateway(192, 168, 233, 1);     // Telefonun IP’si (varsayılan)
  IPAddress subnet(255, 255, 255, 0);      // Subnet mask

  WiFi.config(local_IP, gateway, subnet);  // Statik IP tanımlaması

  WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
  Serial.print("WiFi'ye bağlanılıyor");
  int timeout = 20;
  while (WiFi.status() != WL_CONNECTED && timeout-- > 0) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi bağlantısı başarılı!");
    Serial.println(WiFi.localIP());
    initCamera();
    startCameraServer();
    wifiConnected = true;
  } else {
    Serial.println("\nBağlantı başarısız!");
  }
}

void setupSoftAP() {
  WiFi.softAP(ap_ssid, ap_password);
  Serial.println("SoftAP açıldı.");
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  setupServer.on("/", HTTP_GET, []() {
    setupServer.send(200, "text/html",
      "<!DOCTYPE html>"
      "<html>"
      "<head>"
      "<meta charset='UTF-8'>"
      "<title>WiFi Configuration</title>"
      "<style>"
      "body { display: flex; justify-content: center; align-items: center; height: 100vh; margin: 0; background-color: #f0f0f0; font-family: Arial, sans-serif; }"
      "form { width: 320px; padding: 20px; box-sizing: border-box; background: white; border-radius: 10px; box-shadow: 0 0 10px rgba(0, 0, 0, 0.1); }"
      "h3 { margin-top: 0; margin-bottom: 15px; text-align: center; }"
      "input[type='text'], input[type='password'], input[type='submit'] { box-sizing: border-box; width: 100%; padding: 10px; margin-bottom: 15px; border: 1px solid #ccc; border-radius: 5px; }"
      "input[type='submit'] { background-color: #007BFF; color: white; border: none; cursor: pointer; }"
      "input[type='submit']:hover { background-color: #0056b3; }"
      "</style>"
      "</head>"
      "<body>"
      "<form action='/connect' method='POST'>"
      "<h3>WiFi Settings</h3>"
      "<label>SSID:</label><input type='text' name='ssid' required>"
      "<label>Password:</label><input type='password' name='password' required>"
      "<input type='submit' value='Connect'>"
      "</form>"
      "</body>"
      "</html>");
  });

  setupServer.on("/connect", HTTP_POST, []() {
    wifi_ssid = setupServer.arg("ssid");
    wifi_password = setupServer.arg("password");

    setupServer.send(200, "text/html", "<h3>Bağlanıyor...</h3>");
    setupServer.stop();  // Server'ı durduruyoruz
    WiFi.softAPdisconnect(true);  // SoftAP'yi kapatıyoruz
    tryConnectAndStartCamera();
  });

  setupServer.begin();
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  setupSoftAP();
}

void loop() {
  if (!wifiConnected) {
    setupServer.handleClient();
  }
}

