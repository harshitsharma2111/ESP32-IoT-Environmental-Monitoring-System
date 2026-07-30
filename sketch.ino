#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>

#define DHTPIN 19         
#define RELAY_PIN 23      
#define SOIL_PIN 34       
#define DHTTYPE DHT22

DHT dht(DHTPIN, DHTTYPE);
WebServer server(80);

volatile float currentTemp = 0.0;
volatile float currentHumid = 0.0;
volatile int currentMoisture = 0;
volatile bool pumpState = false;

SemaphoreHandle_t xMutex;

const char* ssid = "Wokwi-GUEST";
const char* password = "";

void setup() {
  Serial.begin(115200);
  delay(1000); // Give serial monitor time to open
  
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  
  dht.begin();
  xMutex = xSemaphoreCreateMutex();

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected! IP Address: 10.10.0.2");

  server.on("/", handleRoot);
  server.begin();

  // Create Tasks
  xTaskCreatePinnedToCore(sensorTask, "Sensors", 3072, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(networkTask, "Network", 4096, NULL, 1, NULL, 1);
}

void loop() {
  // Kept empty for FreeRTOS
}

void handleRoot() {
  float t, h;
  int m;
  bool p;

  if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE) {
    t = currentTemp;
    h = currentHumid;
    m = currentMoisture;
    p = pumpState;
    xSemaphoreGive(xMutex);
  }

  String html = "<!DOCTYPE html><html><head><meta http-equiv='refresh' content='3'>";
  html += "<style>body{font-family:Arial; text-align:center; background:#f4f6f9; color:#333;}";
  html += ".card{background:white; padding:20px; border-radius:10px; margin:15px auto; width:300px; box-shadow:0 4px 8px rgba(0,0,0,0.1);}";
  html += ".status{font-weight:bold; color:" + String(p ? "#2ec4b6" : "#e71d36") + ";}</style>";
  html += "<title>Smart Farm Dashboard</title></head><body>";
  html += "<h1>Eco-Agri Smart Dashboard</h1>";
  html += "<div class='card'><h2>🌡️ Temperature</h2><h3>" + String(t, 1) + " &deg;C</h3></div>";
  html += "<div class='card'><h2>💧 Air Humidity</h2><h3>" + String(h, 1) + " %</h3></div>";
  html += "<div class='card'><h2>🌱 Soil Moisture</h2><h3>" + String(m) + " %</h3></div>";
  html += "<div class='card'><h2>💧 Irrigation Pump</h2><h3>Status: <span class='status'>" + String(p ? "RUNNING" : "OFF") + "</span></h3></div>";
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

void sensorTask(void * parameter) {
  while(1) {
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    int rawMoisture = analogRead(SOIL_PIN);
    int m = map(rawMoisture, 0, 4095, 0, 100); 

    // Automatic pump logic rule execution
    bool nextPumpState = pumpState;
    if (m < 35) {
      digitalWrite(RELAY_PIN, HIGH);
      nextPumpState = true;
    } else if (m >= 60) {
      digitalWrite(RELAY_PIN, LOW);
      nextPumpState = false;
    }

    if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE) {
      if (!isnan(h)) currentHumid = h;
      if (!isnan(t)) currentTemp = t;
      currentMoisture = m;
      pumpState = nextPumpState;
      xSemaphoreGive(xMutex);
    }

    // 🚀 FORCE PRINT UNCONDITIONALLY EVERY 2 SECONDS
    Serial.println("\n=== AgriCore-IoT Live Telemetry ===");
    Serial.print("🌡️ Temp: "); Serial.print(t, 1); Serial.println(" °C");
    Serial.print("💧 Humid: "); Serial.print(h, 1); Serial.println(" %");
    Serial.print("🌱 Soil Moisture: "); Serial.print(m); Serial.println(" %");
    Serial.print("⚡ Pump Actuator: "); Serial.println(nextPumpState ? "ON (LED LIT)" : "OFF");
    Serial.println("====================================");

    vTaskDelay(pdMS_TO_TICKS(2000)); 
  }
}

void networkTask(void * parameter) {
  while(1) {
    server.handleClient();
    vTaskDelay(pdMS_TO_TICKS(5)); 
  }
}
