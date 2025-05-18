#pragma once

#include "CameraESP32S3.hpp"
#include <PubSubClient.h>
#include <../secrets.h>
#include <string>
#include <memory>

using std::string;
class ConnectionManager
{
private:
    WiFiClient espClient;
    HTTPClient http;
    PubSubClient client;
    std::unique_ptr<Camera> camera;

    const char *serverUrl; // replace with ip of the device that the server is running on
    const char *logUrl;    // replace with ip of the device that the server is running on
    bool sendFrameFlag = false;

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

public:

    ConnectionManager(std::unique_ptr<Camera> cam) : espClient(), http(), client(espClient), camera(std::move(cam))
    {
        serverUrl = "http://192.168.0.137:8080/upload";
        logUrl = "http://192.168.0.137:8080/log";
    }

    ConnectionManager(const char *serverUrl, const char *logUrl, std::unique_ptr<Camera> cam) : espClient(), http(), client(espClient), serverUrl(serverUrl), logUrl(logUrl), camera(std::move(cam)) {}

    void clientInit()
    {
        client.setServer(mqtt_server, 1883);
        client.setCallback([this](char *topic, byte *payload, unsigned int length)
                           { this->callback(topic, payload, length); });
    }
    inline void cm_startCamera() const noexcept
    {
        camera->startCamera();
    }
    inline void cm_stopCamera() const noexcept
    {
        camera->stopCamera();
    }
    void cm_sendframeToServer()
    {
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb)
        {
            Serial.println("Failed to capture frame");
            return;
        }

        http.begin(serverUrl);
        http.addHeader("Content-Type", "image/jpeg");

        int httpResponseCode = http.POST(fb->buf, fb->len);

        if (httpResponseCode <= 0)
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
                camera->startCamera();
                sendFrameFlag = true;
            }
            else if (command == 0)
            {
                Serial.println("Stopping frame capture");
                sendFrameFlag = false;
                camera->stopCamera();
            }
        }
    }
    void sendLogToServer(const char *message)
    {
        http.begin(logUrl);
        http.addHeader("Content-Type", "text/plain");

        int httpResponseCode = http.POST((uint8_t *)message, strlen(message));

        if (httpResponseCode <= 0)
        {
            // Error sending log
        }

        http.end();
    }

    inline void is_connected()
    {
        if (!client.connected())
        {
            reconnect();
        }
    }
    inline void client_loop()
    {
        client.loop();
    }
};