#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>
#include <esp_now.h>
#include <esp_wifi.h>

// make sure to put in your wifi cred. here before you run

// cam code dont ask me how it works i used a sample that fernando showed
// just changed the frontend thing to be simple
// with some help of course

const char* ssid = "********";
const char* password = "********";

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

// Built-in ESP32-CAM flash LED.
// On the common AI Thinker ESP32-CAM, this is GPIO 4.
#define FLASH_LED_GPIO_NUM 4

const int LED_PWM_FREQ = 5000;
const int LED_PWM_RESOLUTION = 8; // brightness range: 0-255

WebServer server(80);
WiFiServer streamServer(81);

typedef struct {
  float temperature;
  float humidity;
} SensorData;

float temp = 0;
float hum = 0;
bool hasData = false;

// LED brightness values:
// 0   = off
// 20  = low
// 100 = medium
// 220 = high
// 255 = full brightness

void handleLedOff() {
  ledcWrite(FLASH_LED_GPIO_NUM, 0);
  server.send(200, "text/plain", "LED OFF");
}

void handleLedLow() {
  ledcWrite(FLASH_LED_GPIO_NUM, 20);
  server.send(200, "text/plain", "LED LOW");
}

void handleLedMed() {
  ledcWrite(FLASH_LED_GPIO_NUM, 100);
  server.send(200, "text/plain", "LED MEDIUM");
}

void handleLedHigh() {
  ledcWrite(FLASH_LED_GPIO_NUM, 220);
  server.send(200, "text/plain", "LED HIGH");
}

bool initCamera() {
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

  if (psramFound()) {
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 16;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_QQVGA;
    config.jpeg_quality = 20;
    config.fb_count = 1;
  }

  esp_err_t err = esp_camera_init(&config);

  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    return false;
  }

  sensor_t *s = esp_camera_sensor_get();

  if (s) {
    s->set_vflip(s, 1);
    // s->set_hmirror(s, 1);
  }

  return true;
}

void onReceive(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (len == sizeof(SensorData)) {
    SensorData d;
    memcpy(&d, data, sizeof(d));

    temp = d.temperature;
    hum = d.humidity;
    hasData = true;

    Serial.printf("Temp: %.1f F | Hum: %.1f\n", temp, hum);
  }
}

void handleSensor() {
  if (!hasData) {
    server.send(200, "application/json", "{\"status\":\"waiting\"}");
    return;
  }

  String json = "{";
  json += "\"t\":" + String(temp, 1) + ",";
  json += "\"h\":" + String(hum, 1);
  json += "}";

  server.send(200, "application/json", json);
}

void handleRoot() {
  String ip = WiFi.localIP().toString();

  String html = "<!DOCTYPE html><html><head>"
                "<meta name='viewport' content='width=device-width, initial-scale=1'>"
                "<title>Robot Camera</title>"
                "<style>"
                "body{background:#111;color:white;font-family:Arial;text-align:center;margin:0;padding:8px;}"
                "img{width:100%;max-width:480px;border:1px solid #555;}"
                "p{color:#aaa;font-size:13px;}"
                "button{font-size:16px;padding:10px 14px;margin:5px;border-radius:8px;border:none;}"
                "#s{color:#ddd;font-size:16px;}"
                "</style></head><body>";

  html += "<h3>Robot Camera</h3>";
  html += "<img src='http://" + ip + ":81/stream'>";
  html += "<p>Use the physical joystick ESP32 for driving.</p>";

  html += "<h3>Camera LED Flash</h3>";
  html += "<div style='margin-top:12px;'>";
  html += "<button onclick=\"fetch('/led/off')\">LED OFF</button>";
  html += "<button onclick=\"fetch('/led/low')\">LOW</button>";
  html += "<button onclick=\"fetch('/led/med')\">MED</button>";
  html += "<button onclick=\"fetch('/led/high')\">HIGH</button>";
  html += "</div>";

  html += "<h3>Sensor</h3>";
  html += "<p id='s'>Loading...</p>";

  html += "<script>";
  html += "async function u(){";
  html += "let r=await fetch('/sensor');";
  html += "let d=await r.json();";
  html += "if(d.status){document.getElementById('s').innerHTML='Waiting...';}";
  html += "else{document.getElementById('s').innerHTML='Temp: '+d.t+' Fahrenheit<br>Humidity: '+d.h+' %';}";
  html += "}";
  html += "setInterval(u,2000);u();";
  html += "</script>";

  html += "</body></html>";

  server.send(200, "text/html", html);
}

void handleStreamTask(void *parameter) {
  while (true) {
    WiFiClient client = streamServer.available();

    if (client) {
      Serial.println("Stream client connected");

      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: multipart/x-mixed-replace; boundary=frame");
      client.println();

      while (client.connected()) {
        camera_fb_t *fb = esp_camera_fb_get();

        if (!fb) {
          Serial.println("Camera capture failed");
          break;
        }

        client.println("--frame");
        client.println("Content-Type: image/jpeg");
        client.print("Content-Length: ");
        client.println(fb->len);
        client.println();

        client.write(fb->buf, fb->len);
        client.println();

        esp_camera_fb_return(fb);

        yield();
        vTaskDelay(50 / portTICK_PERIOD_MS);
      }

      client.stop();
      Serial.println("Stream client disconnected");
    }

    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("Booting ESP32-CAM...");

  if (!initCamera()) {
    Serial.println("Camera init failed");
    return;
  }

  // Set up the built-in ESP32-CAM flash LED using PWM.
  ledcAttach(FLASH_LED_GPIO_NUM, LED_PWM_FREQ, LED_PWM_RESOLUTION);
  ledcWrite(FLASH_LED_GPIO_NUM, 0); // LED off at startup

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connected");

  Serial.print("Camera ESP IP: ");
  Serial.println(WiFi.localIP());

  Serial.print("ESP32-CAM MAC: ");
  Serial.println(WiFi.macAddress());

  Serial.print("WiFi channel: ");
  Serial.println(WiFi.channel());

  // Webpage route
  server.on("/", handleRoot);

  // Sensor JSON route
  server.on("/sensor", handleSensor);

  // LED brightness routes
  server.on("/led/off", handleLedOff);
  server.on("/led/low", handleLedLow);
  server.on("/led/med", handleLedMed);
  server.on("/led/high", handleLedHigh);

  server.begin();
  streamServer.begin();

  xTaskCreatePinnedToCore(
    handleStreamTask,
    "StreamTask",
    8192,
    NULL,
    1,
    NULL,
    0
  );

  Serial.println("HTTP server started");
  Serial.println("Stream server started");

  if (esp_now_init() == ESP_OK) {
    esp_now_register_recv_cb(onReceive);
    Serial.println("ESP-NOW receive ready");
  } else {
    Serial.println("ESP-NOW init failed");
  }
}

void loop() {
  server.handleClient();
  delay(1);
}