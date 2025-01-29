from flask import Flask, Response, stream_with_context, request
import cv2
import numpy as np
import mediapipe as mp
import paho.mqtt.client as mqtt
from credentials import mqtt_broker, mqtt_user, mqtt_password, mqtt_port, mqtt_topic
import threading
import time

from mediapipe.tasks import python
from mediapipe.tasks.python import vision

app = Flask(__name__)

# Frame buffer to hold the latest frame
latest_frame = None

# Initialize GestureRecognizer
VisionRunningMode = mp.tasks.vision.RunningMode
base_options = python.BaseOptions(model_asset_path='gesture_recognizer.task')
options = vision.GestureRecognizerOptions(base_options=base_options, min_hand_detection_confidence=0.3)
recognizer = vision.GestureRecognizer.create_from_options(options)

# Initialize Mediapipe Face Detection
mp_face_detection = mp.solutions.face_detection
mp_drawing = mp.solutions.drawing_utils
face_detection = mp_face_detection.FaceDetection(min_detection_confidence=0.5)

mp_hands_data = mp.solutions.hands
mp_hands = mp_hands_data.Hands(static_image_mode=False,
                               max_num_hands=2,
                               min_detection_confidence=0.5,
                               min_tracking_confidence=0.5)

# MQTT Client setup
error_messages = {
    0: "Connected to MQTT Broker successfully",
    1: "Connection refused - incorrect protocol version",
    2: "Connection refused - invalid client identifier",
    3: "Connection refused - server unavailable",
    4: "Connection refused - bad username or password",
    5: "Connection refused - not authorized"
}


# Callback function for connection result
def on_connect(client, userdata, flags, rc):
    print(error_messages.get(rc, f"Connection failed with error code {rc}"))

# Function to establish connection with retry mechanism
def connect_with_retries(client, broker, port, retry_interval=30):
    while True:
        try:
            print("Attempting to connect to MQTT broker...")
            client.connect(broker, port, 60)
            client.loop_start()  # Start loop to process network traffic
            return
        except Exception as e:
            print(f"Failed to connect to MQTT broker: {e}. Retrying in {retry_interval} seconds...")
            time.sleep(retry_interval)

# MQTT Client setup
mqtt_client = mqtt.Client()

# Set username and password for MQTT
mqtt_client.username_pw_set(mqtt_user, mqtt_password)

# Assign callback function
mqtt_client.on_connect = on_connect

# Connect with retries
connect_with_retries(mqtt_client, mqtt_broker, mqtt_port)

# Variable to track last published gesture
last_published_gesture = None

def process_frames():
    global latest_frame
    global last_published_gesture
    while True:
        if latest_frame:
            # Decode JPEG frame to OpenCV format
            np_frame = np.frombuffer(latest_frame, np.uint8)
            frame = cv2.imdecode(np_frame, cv2.IMREAD_COLOR)

            # Process frame for face detection
            if frame is not None:
                # Convert BGR to RGB for Mediapipe
                rgb_frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
                results_face = face_detection.process(rgb_frame)
                results_hand = mp_hands.process(rgb_frame)

                # Draw bounding boxes if faces are detected
                if results_face.detections:
                    for detection in results_face.detections:
                        bboxC = detection.location_data.relative_bounding_box
                        ih, iw, _ = frame.shape
                        x, y, w, h = int(bboxC.xmin * iw), int(bboxC.ymin * ih), int(bboxC.width * iw), int(
                            bboxC.height * ih)
                        # Draw rectangle around the face
                        cv2.rectangle(frame, (x, y), (x + w, y + h), (0, 255, 0), 2)

                # Convert the frame to a Mediapipe Image format
                mp_image = mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb_frame)

                # Perform gesture recognition
                recognition_result = recognizer.recognize(mp_image)

                # If gestures are detected
                if recognition_result.gestures:
                    top_gesture = recognition_result.gestures[0][0]  # Get the most confident gesture
                    gesture_name = top_gesture.category_name
                    confidence = top_gesture.score

                    # If hand landmarks are available, draw them as well
                    if results_hand.multi_hand_landmarks:
                        for hand_landmarks, hand_handeness in zip(results_hand.multi_hand_landmarks,
                                                                  results_hand.multi_handedness):
                            handedness = hand_handeness.classification[0].label  # "Right" or "Left"
                            mp_drawing.draw_landmarks(
                                frame, hand_landmarks, mp_hands_data.HAND_CONNECTIONS)
                            if handedness == "Right":
                                if "_L" not in gesture_name:
                                    gesture_name += "_L"
                            elif handedness == "Left":
                                if "_R" not in gesture_name:
                                    gesture_name += "_R"
                    # Display gesture name and confidence
                    cv2.putText(frame, f'{gesture_name} ({confidence:.2f})', (10, 30),
                                cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)
                    # Avoid publishing if gesture is the same as the last one
                    if gesture_name != last_published_gesture and "None" not in gesture_name:
                        # Publish gesture to MQTT
                        mqtt_client.publish(mqtt_topic, gesture_name)
                        last_published_gesture = gesture_name
                        print(f"Published gesture: {gesture_name}")
                    elif gesture_name == "None":
                        print("No gesture detected. Skipping publishing.")
        time.sleep(0.1)  # Add a small delay to avoid high CPU usage

@app.route('/upload', methods=['POST'])
def upload_frame():
    global latest_frame
    if request.data:
        latest_frame = request.data  # Store the latest frame in memory
        return "Frame received", 200
    else:
        return "No frame received", 400

@app.route('/stream', methods=['GET'])
def stream_frames():
    @stream_with_context
    def generate():
        global latest_frame
        while True:
            if latest_frame:
                # Decode JPEG frame to OpenCV format
                np_frame = np.frombuffer(latest_frame, np.uint8)
                frame = cv2.imdecode(np_frame, cv2.IMREAD_COLOR)

                # Process frame for face detection
                if frame is not None:
                    # Convert BGR to RGB for Mediapipe
                    rgb_frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
                    results_face = face_detection.process(rgb_frame)
                    results_hand = mp_hands.process(rgb_frame)

                    # Draw bounding boxes if faces are detected
                    if results_face.detections:
                        for detection in results_face.detections:
                            bboxC = detection.location_data.relative_bounding_box
                            ih, iw, _ = frame.shape
                            x, y, w, h = int(bboxC.xmin * iw), int(bboxC.ymin * ih), int(bboxC.width * iw), int(
                                bboxC.height * ih)
                            # Draw rectangle around the face
                            cv2.rectangle(frame, (x, y), (x + w, y + h), (0, 255, 0), 2)

                    # If hand landmarks are available, draw them as well
                    if results_hand.multi_hand_landmarks:
                        for hand_landmarks in results_hand.multi_hand_landmarks:
                            mp_drawing.draw_landmarks(
                                frame, hand_landmarks, mp_hands_data.HAND_CONNECTIONS)

                    # Encode frame back to JPEG
                    _, jpeg_frame = cv2.imencode('.jpg', frame)

                    # Stream the modified frame
                    yield (b'--frame\r\n'
                           b'Content-Type: image/jpeg\r\n\r\n' + jpeg_frame.tobytes() + b'\r\n')

    return Response(generate(), mimetype='multipart/x-mixed-replace; boundary=frame')

if __name__ == '__main__':
    # Start the frame processing thread
    frame_processing_thread = threading.Thread(target=process_frames)
    frame_processing_thread.daemon = True
    frame_processing_thread.start()

    app.run(host='0.0.0.0', port=8080, debug=True)
