#include "Camera.hpp"

class CameraESP32S3 : public Camera
{
private:
    camera_config_t config;

public:
    CameraESP32S3();
    void startCamera() const override;
    void stopCamera() const override;
};