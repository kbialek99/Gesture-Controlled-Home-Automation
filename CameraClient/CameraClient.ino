#include <WiFi.h>
#include <esp_camera.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <secrets.h>
#include <esp_sleep.h>

#define CAMERA_MODEL_XIAO_ESP32S3 // Has PSRAM

#define PIN_HIGH 1
#define PIN_LOW 0
#define MOTION_DETECTED_THRESHOLD 1

#include "camera_pins.h"

const char *serverUrl = "http://192.168.0.137:8080/upload"; // replace with ip of the device that the server is running on
const char *logUrl = "http://192.168.0.137:8080/log";       // replace with ip of the device that the server is running on

#define PIR_PIN GPIO_NUM_7 // Define the pin for the PIR sensor

WiFiClient espClient;
PubSubClient client(espClient);
bool sendFrameFlag = false;
unsigned long lastMotionTime = 0;
int motionDetectedCount = 0;

void sendLogToServer(const char *message)
{
  HTTPClient http;
  http.begin(logUrl);
  http.addHeader("Content-Type", "text/plain");

  int httpResponseCode = http.POST((uint8_t *)message, strlen(message));

  if (httpResponseCode > 0)
  {
    // Log sent successfully
  }
  else
  {
    // Error sending log
  }

  http.end();
}

void startCamera()
{
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
  config.xclk_freq_hz = 20000000;       // Keep the clock frequency at 20 MHz
  config.frame_size = FRAMESIZE_VGA;    // Use VGA (640x480) for good balance of detail and size
  config.pixel_format = PIXFORMAT_JPEG; // Use JPEG for compressed frames if bandwidth is limited
  // Alternative: Use PIXFORMAT_JPEG for compressed frames if bandwidth is limited
  config.grab_mode = CAMERA_GRAB_LATEST;   // Grab the latest frame to reduce latency
  config.fb_location = CAMERA_FB_IN_PSRAM; // Use PSRAM for frame buffering
  config.jpeg_quality = 15;                // Medium JPEG quality (lower number = better quality)
  config.fb_count = 2;                     // Double buffering for smoother frame capture

  // Initialize camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK)
  {
    Serial.printf("Camera initialization failed! Error code: 0x%x\n", err);
    while (true)
      ;
  }
  else
  {
    sendLogToServer("Camera initialization successful!");
  }
}

void stopCamera()
{
  esp_camera_deinit();
  sendLogToServer("Camera deinitialized to reduce heating.");
}

void sendFrameToServer()
{
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb)
  {
    Serial.println("Failed to capture frame");
    return;
  }

  HTTPClient http;
  http.begin(serverUrl);
  http.addHeader("Content-Type", "image/jpeg");

  int httpResponseCode = http.POST(fb->buf, fb->len);

  if (httpResponseCode > 0)
  {
    // Serial.printf("Frame sent successfully! HTTP Response code: %d\n", httpResponseCode);
  }
  else
  {
    Serial.printf("Error sending frame: %s\n", http.errorToString(httpResponseCode).c_str());
  }

  http.end();
  esp_camera_fb_return(fb);
}

void callback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  payload[length] = '\0'; // Null-terminate the payload
  Serial.println((char *)payload);

  if (strcmp(topic, "wakeup/espCamera") == 0)
  {
    int command = atoi((char *)payload);
    if (command == 1)
    {
      Serial.println("Starting frame capture");
      startCamera();
      sendFrameFlag = true;
    }
    else if (command == 0)
    {
      Serial.println("Stopping frame capture");
      sendFrameFlag = false;
      stopCamera();
    }
  }
}

void reconnect()
{
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP32CameraClient", mqtt_user, mqtt_password))
    {
      Serial.println("connected");
      client.subscribe("wakeup/espCamera");
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}
bool motionState = false;

void detectMotion()
{
  Serial.println("Motion detected!");
  motionDetectedCount++;
  lastMotionTime = millis();
  if (motionDetectedCount >= MOTION_DETECTED_THRESHOLD && motionState == false)
  {
    motionState = true;
    // startCamera();
  }
}

void setup()
{
  Serial.begin(115200);
  WiFi.begin(ssid, password);

  Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
  Serial.printf("Free PSRAM: %d bytes\n", ESP.getFreePsram());

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  pinMode(PIR_PIN, INPUT_PULLUP); // Ensure the pin is properly configured

  // Check wake-up reason
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  switch (wakeup_reason)
  {
  case ESP_SLEEP_WAKEUP_EXT0:
    Serial.println("Wakeup caused by external signal using RTC_IO");
    sendLogToServer("Wakeup caused by external signal using RTC_IO");
    break;
  case ESP_SLEEP_WAKEUP_EXT1:
    Serial.println("Wakeup caused by external signal using RTC_CNTL");
    sendLogToServer("Wakeup caused by external signal using RTC_CNTL");
    break;
  default:
    Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason);
    sendLogToServer("Wakeup was not caused by deep sleep");
    break;
  }

  pinMode(PIR_PIN, INPUT);

  startCamera();
}

void loop()
{
  static unsigned long lastReadTime = 0;
  unsigned long currentTime = millis();
  static unsigned long lastTime = 0;
  static int frameCount = 0;
  if (!client.connected())
  {
    reconnect();
  }
  client.loop();

  // Check PIR sensor state
  if (digitalRead(PIR_PIN) == HIGH)
  {
    detectMotion();
  }
  else if (digitalRead(PIR_PIN) == LOW)
  {
    Serial.println("No motion detected");
  }

  sendFrameToServer();
  // Count frames per second
  frameCount++;
  if (currentTime - lastTime >= 1000)
  { // Every second
    char logMessage[50];
    snprintf(logMessage, sizeof(logMessage), "FPS: %d", frameCount);
    sendLogToServer(logMessage);
    frameCount = 0;
    lastTime = currentTime;
  }

  if (millis() - lastMotionTime >= 10000)
  { // No motion detected for 10 seconds
    motionDetectedCount = 0;
    motionState = false;
    sendLogToServer("No motion detected for 10 seconds, going to deep sleep.");
    Serial.println("Entering deep sleep...");        // Debugging line
    stopCamera();                                    // Deinitialize camera to reduce heating
    delay(100);                                      // Ensure the log message is sent before sleeping
    esp_sleep_enable_ext0_wakeup(PIR_PIN, PIN_HIGH); // Wake up when motion is detected (rising edge)
    esp_deep_sleep_start();
  }
  delay(15);
}
