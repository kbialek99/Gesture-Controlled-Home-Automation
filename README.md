# Gesture-Controlled Home Automation

This project enables gesture-based home automation using an ESP32 camera, Raspberry Pi, Mediapipe, and Home Assistant. It detects hand gestures from a live video feed and publishes them to an MQTT broker, allowing integration with Home Assistant for various automation tasks.

## Features

- **Live Gesture Recognition**: Uses Mediapipe's Gesture Recognizer to identify hand gestures in real-time.
- **ESP32-CAM Integration**: Streams video frames from an ESP32-CAM to a Flask-based web server running on a Raspberry Pi.
- **MQTT Integration**: Publishes detected gestures to a specific MQTT topic (`/gestureControl/input`) for use in Home Assistant automations.
- **Home Assistant Automation**: Gestures can trigger custom automations, such as turning on lights, adjusting thermostats, or activating smart devices.

## How It Works

1. **ESP32-CAM**: Captures video frames and sends them to the Raspberry Pi's web server via HTTP POST requests.
2. **Web Server**: A Flask-based Python server processes the frames using Mediapipe's Gesture Recognizer and detects gestures.
3. **Gesture Publishing**: Detected gestures are sent to the MQTT broker, ensuring only unique gestures (not "None") are published.
4. **Home Assistant**: Subscribes to the MQTT topic and uses the detected gestures in automations.

https://github.com/user-attachments/assets/40108a77-0399-4302-a722-31a51820c4ff

## Setup Instructions

### Hardware Requirements

- ESP32-CAM module (Xiao ESP32-S3 with OV2540 camera module)
- Any device that can run a python script, ideally one that runs all the time (tested on Raspberry Pi 5)
- Home Assistant instance with MQTT broker integration installed

### Software Requirements

- Python 3.10 (newer versions do not support Mediapipe)
- Mediapipe
- Flask
- OpenCV
- Paho MQTT
- Docker (optional for containerization)

### Installation

1. **Clone the Repository**
   ```bash
   git clone https://github.com/your-username/gesture-controlled-home-automation.git
   cd gesture-controlled-home-automation
   ```

2. **Build Docker Container**
   ```bash
   cd GestureProcessingServer
   docker build -t gesture-processing-server .
   ```

3. **Run Docker Container**
   ```bash
   sudo docker run -p 8080:8080 gesture-recognition-app
   ```
   copy the line above and open the bashrc file with
   ```
   nano ~/.bashrc
   ```

   and paste it to the end of the file. This will make the docker container run on system startup 

4. **Configure ESP32-CAM**

   Ensure all credentials for WiFi, MQTT server IP address are set properly. Flash the device with the camera firmware. Make sure to flash with PSRAM enabled if using ESP32-S3.

6. **Open Stream in Browser**

   Open the stream in your browser to verify the setup:
   ```
   http://192.168.0.xxx:8080/stream
   ```

8. **Home Assistant Setup**

   Home Assistant should be able to receive payloads on the topic `gestureControl/input` at this point.
