#include <WiFi.h>
#include "../include/Camera.hpp"
#include <PubSubClient.h>
#include <esp_sleep.h>
#include "../include/Proxy.hpp"

#define PIN_HIGH 1
#define PIN_LOW 0
#define MOTION_DETECTED_THRESHOLD 1


#define PIR_PIN GPIO_NUM_4 // Define the pin for the PIR sensor

bool sendFrameFlag = false;
unsigned long lastMotionTime = 0;
int motionDetectedCount = 0;
ConnectionManager connectionManager;

bool motionState = false;

void detectMotion()
{
  Serial.println("Pin HIGH");
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

    connectionManager.clientInit();
  }
  Serial.println();
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());

  pinMode(PIR_PIN, INPUT_PULLDOWN); // Ensure the pin is properly configured

  // Check wake-up reason
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  switch (wakeup_reason)
  {
  case ESP_SLEEP_WAKEUP_EXT0:
    Serial.println("Wakeup caused by external signal using RTC_IO");
    //sendLogToServer("Wakeup caused by external signal using RTC_IO");
    break;
  case ESP_SLEEP_WAKEUP_EXT1:
    Serial.println("Wakeup caused by external signal using RTC_CNTL");
    //sendLogToServer("Wakeup caused by external signal using RTC_CNTL");
    break;
  default:
    Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason);
    //sendLogToServer("Wakeup was not caused by deep sleep");
    break;
  }

  //pinMode(PIR_PIN, INPUT);
  connectionManager.cm_startCamera();
}

void loop()
{
  static unsigned long lastReadTime = 0;
  unsigned long currentTime = millis();
  static unsigned long lastTime = 0;
  static int frameCount = 0;

  connectionManager.is_connected();
  connectionManager.client_loop();

  // Check PIR sensor state
  if (digitalRead(PIR_PIN) == HIGH)
  {
    detectMotion();
  }
  else if (digitalRead(PIR_PIN) == LOW)
  {
    Serial.println("Pin LOW");
  }
  // Send the frame to the server
  connectionManager.cm_sendframeToServer();
  // Count frames per second
  frameCount++;
  if (currentTime - lastTime >= 1000)
  { // Every second
    char logMessage[50];
    snprintf(logMessage, sizeof(logMessage), "FPS: %d", frameCount);
    //sendLogToServer(logMessage);
    frameCount = 0;
    lastTime = currentTime;
  }

  if (millis() - lastMotionTime >= 10000)
  { // No motion detected for 10 seconds
    motionDetectedCount = 0;
    motionState = false;
    //sendLogToServer("No motion detected for 10 seconds, going to deep sleep.");
    Serial.println("Entering deep sleep...");        // Debugging line
    connectionManager.cm_stopCamera();                                    // Deinitialize camera to reduce heating
    delay(100);                                      // Ensure the log message is sent before sleeping
    esp_sleep_enable_ext0_wakeup(PIR_PIN, PIN_HIGH); // Wake up when motion is detected (rising edge)
    esp_deep_sleep_start();
  }
  delay(15);
}
