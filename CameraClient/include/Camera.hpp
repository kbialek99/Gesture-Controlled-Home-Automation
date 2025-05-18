#include <esp_camera.h>
#include <HTTPClient.h>

#pragma once
//#include "../Logger.hpp"
#define CAMERA_MODEL_XIAO_ESP32S3 // Has PSRAM

#include "camera_pins.h"

class Camera {

public:
    Camera() = default;
    virtual ~Camera() = default; 
    virtual void startCamera() const = 0;
    virtual void stopCamera() const = 0;
};