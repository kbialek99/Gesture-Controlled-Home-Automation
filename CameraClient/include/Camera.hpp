#include <esp_camera.h>
#include <HTTPClient.h>

#pragma once
//#include "../Logger.hpp"
#define CAMERA_MODEL_XIAO_ESP32S3 // Has PSRAM

#include "camera_pins.h"

class Camera {
private:
    camera_config_t config;
    static Camera* instancePtr;
    Camera();

public:
    Camera(Camera const&) = delete;
    
    static Camera* getInstance() {
        if(instancePtr == nullptr){
            instancePtr = new Camera();
        }
        return instancePtr;
    }
    void startCamera() const;
    void stopCamera() const ;
};