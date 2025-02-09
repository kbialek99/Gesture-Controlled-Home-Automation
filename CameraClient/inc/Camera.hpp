#include <esp_camera.h>
#include <HTTPClient.h>

//#include "../Logger.hpp"
#define CAMERA_MODEL_XIAO_ESP32S3 // Has PSRAM

#include "camera_pins.h"



class Camera {
private:
    camera_config_t config;
    const char *serverUrl = "http://192.168.0.137:8080/upload"; // replace with ip of the device that the server is running on

    //Logger logger = Logger::getInstance();

public:
    Camera();
    void startCamera();
    void sendFrameToServer();
    void stopCamera();
};