from flask import Flask, Response, stream_with_context, request
import cv2
import numpy as np
import mediapipe as mp
from GestureClassification import is_thumbs_up

from mediapipe.tasks import python
from mediapipe.tasks.python import vision

app = Flask(__name__)


# Frame buffer to hold the latest frame
latest_frame = None

# Initialize GestureRecognizer
VisionRunningMode = mp.tasks.vision.RunningMode
base_options = python.BaseOptions(model_asset_path='gesture_recognizer.task')
options = vision.GestureRecognizerOptions(base_options=base_options, min_hand_detection_confidence=0.3 )
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


                    '''
                    # Draw hand landmarks and bounding boxes
                    if results_hand.multi_hand_landmarks:
                        for hand_landmarks, hand_handeness in zip(results_hand.multi_hand_landmarks, results_hand.multi_handedness):
                            handedness = hand_handeness.classification[0].label  # "Right" or "Left"
                            mp_drawing.draw_landmarks(
                                frame, hand_landmarks, mp_hands_data.HAND_CONNECTIONS)

                            # Extract bounding box coordinates
                            h, w, _ = frame.shape
                            x_min = int(min([lm.x for lm in hand_landmarks.landmark]) * w)
                            y_min = int(min([lm.y for lm in hand_landmarks.landmark]) * h)
                            x_max = int(max([lm.x for lm in hand_landmarks.landmark]) * w)
                            y_max = int(max([lm.y for lm in hand_landmarks.landmark]) * h)

                            if is_thumbs_up(hand_landmarks, handedness):
                                cv2.putText(frame, "Thumbs Up", (x_min, y_min - 10),
                                            cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2, cv2.LINE_AA)

                            # Draw bounding box
                            cv2.rectangle(frame, (x_min, y_min), (x_max, y_max), (0, 255, 0), 2)
                    '''
                    # Convert the frame to a Mediapipe Image format
                    mp_image = mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb_frame)

                    # Perform gesture recognition
                    recognition_result = recognizer.recognize(mp_image)

                    # If gestures are detected
                    if recognition_result.gestures:
                        top_gesture = recognition_result.gestures[0][0]  # Get the most confident gesture
                        gesture_name = top_gesture.category_name
                        confidence = top_gesture.score

                        # Draw gesture label on the frame
                        cv2.putText(frame, f'{gesture_name} ({confidence:.2f})', (10, 30),
                                    cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)

                    # # If hand landmarks are available, draw them as well
                    if results_hand.multi_hand_landmarks:
                        for hand_landmarks, hand_handeness in zip(results_hand.multi_hand_landmarks, results_hand.multi_handedness):
                            handedness = hand_handeness.classification[0].label  # "Right" or "Left"
                            mp_drawing.draw_landmarks(
                                frame, hand_landmarks, mp_hands_data.HAND_CONNECTIONS)


                    # Encode frame back to JPEG
                    _, jpeg_frame = cv2.imencode('.jpg', frame)

                    # Stream the modified frame
                    yield (b'--frame\r\n'
                           b'Content-Type: image/jpeg\r\n\r\n' + jpeg_frame.tobytes() + b'\r\n')

    return Response(generate(), mimetype='multipart/x-mixed-replace; boundary=frame')


if __name__ == '__main__':
    app.run(host='0.0.0.0', port=8080, debug=True)
80