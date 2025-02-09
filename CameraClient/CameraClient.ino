#include <WiFi.h>
#include <secrets.h>
#include "inc/Camera.hpp"
#include <PubSubClient.h>
#include <esp_sleep.h>

#define PIN_HIGH 1
#define PIN_LOW 0
#define MOTION_DETECTED_THRESHOLD 1

const char *serverUrl = "http://192.168.0.137:8080/upload"; // replace with ip of the device that the server is running on
const char *logUrl = "http://192.168.0.137:8080/log";       // replace with ip of the device that the server is running on

#define PIR_PIN GPIO_NUM_1 // Define the pin for the PIR sensor

WiFiClient espClient;
bool sendFrameFlag = false;
PubSubClient client(espClient);
bool sendFrameFlag = false;
unsigned long lastMotionTime = 0;
int motionDetectedCount = 0;
Camera camera;

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
      camera.startCamera();
      sendFrameFlag = true;
    }
    else if (command == 0)
    {
      Serial.println("Stopping frame capture");
      sendFrameFlag = false;
      camera.stopCamera();
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

  pinMode(PIR_PIN, INPUT_PULLDOWN); // Ensure the pin is properly configured

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
  camera.startCamera();
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
  // Send the frame to the server
  camera.sendFrameToServer();
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
    camera.stopCamera();                                    // Deinitialize camera to reduce heating
    delay(100);                                      // Ensure the log message is sent before sleeping
    esp_sleep_enable_ext0_wakeup(PIR_PIN, PIN_HIGH); // Wake up when motion is detected (rising edge)
    esp_deep_sleep_start();
  }
  delay(15);
}
