#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>

Adafruit_MPU6050 mpu;

// SoftAP bilgileri
const char* ap_ssid = "MPU6050-Setup";
const char* ap_password = "12345678";

// Kullanıcının girdiği WiFi bilgileri
String wifi_ssid = "";
String wifi_password = "";

// SoftAP Web sunucusu
WebServer setupServer(80);
bool wifiConnected = false;

// Düşme tespiti parametreleri
const float ACCELERATION_THRESHOLD = 12.0;
const unsigned long COOLDOWN_PERIOD = 5000;
unsigned long lastDetectionTime = 0;

// X ve Z eksenlerine göre yön belirleme fonksiyonu
String detectFallDirection(float ax, float az) {
  float absX = abs(ax);
  float absZ = abs(az);

  String mainDirection = "";
  String secondaryDirection = "";

  if (absX >= absZ) {
    mainDirection = (ax < 0) ? "backward" : "forward";

    if ((absX / 2.0) < absZ) {
      secondaryDirection = (az < 0) ? " + right" : " + left";
    }
  } else {
    mainDirection = (az < 0) ? "right" : "left";

    if ((absZ / 2.0) < absX) {
      secondaryDirection = (ax < 0) ? " + backward" : " + forward";
    }
  }

  return mainDirection + secondaryDirection;
}

// SoftAP başlatma ve arayüz
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

    setupServer.send(200, "text/html", "<h3>Bağlanılıyor...</h3>");
    setupServer.stop();
    WiFi.softAPdisconnect(true);

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
      wifiConnected = true;

      if (!mpu.begin()) {
        Serial.println("MPU6050 bulunamadı!");
        while (1) delay(10);
      }
      Serial.println("MPU6050 başlatıldı!");
    } else {
      Serial.println("\nWiFi bağlantısı başarısız!");
    }
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
    return;
  }

  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  float ax = a.acceleration.x;
  float ay = a.acceleration.y;
  float az = a.acceleration.z;

  float totalAccel = sqrt(ax * ax + ay * ay + az * az);

  Serial.print("X: "); Serial.print(ax);
  Serial.print(" Y: "); Serial.print(ay);
  Serial.print(" Z: "); Serial.print(az);
  Serial.print(" | Total Accel: "); Serial.println(totalAccel);

  if (totalAccel > ACCELERATION_THRESHOLD && millis() - lastDetectionTime > COOLDOWN_PERIOD) {
    lastDetectionTime = millis();
    Serial.println("Düşme algılandı!");

    String fallDirection = detectFallDirection(ax, az);
    Serial.print("Düşme yönü: ");
    Serial.println(fallDirection);

    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      String url = "http://192.168.233.168/capture";  // Burayı kendi sunucu adresinle değiştir
      http.begin(url);
      http.setTimeout(10000); 
      int httpCode = http.GET();

      if (httpCode > 0) {
        Serial.print("HTTP Yanıt Kodu: ");
        Serial.println(httpCode);
      } else {
        Serial.print("HTTP Hatası: ");
        Serial.println(http.errorToString(httpCode).c_str());
      }

      http.end();
    } else {
      Serial.println("WiFi bağlantısı yok, istek gönderilemedi.");
    }
  }

  delay(200);
}
