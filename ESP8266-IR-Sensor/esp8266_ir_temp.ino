
// #include <Wire.h>
// #include <Adafruit_MLX90614.h>

// Adafruit_MLX90614 mlx = Adafruit_MLX90614();
// String command;

// void setup() {
//   Serial.begin(115200);
//   Wire.begin(4, 5); // SDA, SCL pins for ESP8266
//   mlx.begin();
// }

// void loop() {
//   if (Serial.available()) {
//     command = Serial.readStringUntil('\n');
//     command.trim();

//     if (command == "CHECK_TEMP") {
//       float tempC = mlx.readObjectTempC();

//       if (tempC > 30.0) {
//         Serial.println("ALIVE");
//       } else {
//         Serial.println("DEAD");
//       }

//       delay(100); // Delay to prevent spam
//     }
//   }
// }
#include <Wire.h>
#include <Adafruit_MLX90614.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

const char* ssid = "OMDUBEY 1478";
const char* password = "1234567890";

Adafruit_MLX90614 mlx = Adafruit_MLX90614();
ESP8266WebServer server(80);

void setup() {
  Serial.begin(115200);
  Wire.begin(4, 5); // D2=SDA, D1=SCL for ESP8266
  mlx.begin();

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected. IP:");
  Serial.println(WiFi.localIP());

  server.on("/check_temp", HTTP_GET, []() {
    float temp = mlx.readObjectTempC();
    if (temp > 30.0) {
      server.send(200, "text/plain", "ALIVE");
    } else {
      server.send(200, "text/plain", "DEAD");
    }
  });

  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();
}
