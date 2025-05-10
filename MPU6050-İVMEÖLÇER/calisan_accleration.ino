#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>

Adafruit_MPU6050 mpu;

// WiFi bilgileri
const char* ssid = "A33";
const char* password = "123456789";

// Eşik değer ve cooldown süresi
const float ACCELERATION_THRESHOLD = 12.0;
const unsigned long COOLDOWN_PERIOD = 5000;
unsigned long lastDetectionTime = 0;

void setup() {
  Serial.begin(115200);

  // WiFi bağlantısı
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");

  // MPU6050 başlatma
  if (!mpu.begin()) {
    Serial.println("MPU6050 bulunamadı!");
    while (1) delay(10);
  }
  Serial.println("MPU6050 başlatıldı!");
  delay(100);
}

void loop() {
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

    String fallDirection = detectFallDirection(ax, az);  // sadece x ve z ekseniyle yön belirleniyor

    Serial.print("Düşme Algılandı! Yön: ");
    Serial.println(fallDirection);

    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;

      String url = "http://192.168.227.168/capture";
      http.begin(url);

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
      Serial.println("WiFi bağlantısı yok.");
    }
  }

  delay(200);
}
 
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
