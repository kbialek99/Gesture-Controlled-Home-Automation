#include <WiFi.h>
#include <esp_camera.h>
#include <HTTPClient.h>
#include <secrets.h>

#define CAMERA_MODEL_XIAO_ESP32S3 // Has PSRAM

#include "camera_pins.h"

const char *serverUrl = "http://192.168.0.137:80/upload";

void startCamera() {
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

// Adjusted parameters for face/gesture recognition
config.xclk_freq_hz = 20000000;          // Keep the clock frequency at 20 MHz
config.frame_size = FRAMESIZE_VGA;       // Use VGA (640x480) for good balance of detail and size
config.pixel_format = PIXFORMAT_JPEG;  // Use RGB565 for better color and detail (if server processes raw data)
// Alternative: Use PIXFORMAT_JPEG for compressed frames if bandwidth is limited
config.grab_mode = CAMERA_GRAB_LATEST;   // Grab the latest frame to reduce latency
config.fb_location = CAMERA_FB_IN_PSRAM; // Use PSRAM for frame buffering
config.jpeg_quality = 15;                // Medium JPEG quality (lower number = better quality)
config.fb_count = 2;                     // Double buffering for smoother frame capture


  // Initialize camera
  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Camera initialization failed!");
    while (true);
  }
  else
  {
    Serial.println("Camera initialization succesfull!");

  }
}

void sendFrameToServer() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Failed to capture frame");
    return;
  }

  HTTPClient http;
  http.begin(serverUrl);
  http.addHeader("Content-Type", "image/jpeg");

  int httpResponseCode = http.POST(fb->buf, fb->len);

  if (httpResponseCode > 0) {
    //Serial.printf("Frame sent successfully! HTTP Response code: %d\n", httpResponseCode);
  } else {
    Serial.printf("Error sending frame: %s\n", http.errorToString(httpResponseCode).c_str());
  }

  http.end();
  esp_camera_fb_return(fb);
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);

  Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
  Serial.printf("Free PSRAM: %d bytes\n", ESP.getFreePsram());

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());

  startCamera();
}

void loop() {
  static unsigned long lastTime = 0;
  static int frameCount = 0;
  // Send the frame to the server
  sendFrameToServer();
  // Count frames per second
  frameCount++;
  unsigned long currentTime = millis();
  if (currentTime - lastTime >= 1000) {  // Every second
      Serial.printf("FPS: %d\n", frameCount);
      frameCount = 0;
      lastTime = currentTime;
  }
  delay(15);
}
