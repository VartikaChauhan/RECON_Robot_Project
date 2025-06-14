from flask import Flask, jsonify
import cv2
import numpy as np
import torch
import requests

app = Flask(__name__)

# Load YOLOv5 model
model = torch.hub.load('ultralytics/yolov5', 'yolov5s', pretrained=True)

# ESP32-CAM endpoints
ESP32_IP = 'http://<ESP32_IP>'  # Replace with actual IP like 'http://192.168.1.100'
ESP32_STREAM_URL = f'{ESP32_IP}/stream'
ESP32_GPS_URL = f'{ESP32_IP}/gps'

@app.route('/predict', methods=['GET'])
def predict():
    try:
        # Open MJPEG stream
        stream = requests.get(ESP32_STREAM_URL, stream=True, timeout=10)
        bytes_data = bytes()

        for chunk in stream.iter_content(chunk_size=1024):
            bytes_data += chunk
            a = bytes_data.find(b'\xff\xd8')  # JPEG start
            b = bytes_data.find(b'\xff\xd9')  # JPEG end
            if a != -1 and b != -1:
                jpg = bytes_data[a:b+2]
                bytes_data = bytes_data[b+2:]
                img_array = np.frombuffer(jpg, dtype=np.uint8)
                frame = cv2.imdecode(img_array, cv2.IMREAD_COLOR)

                # YOLOv5 detection
                results = model(frame)
                df = results.pandas().xyxy[0]

                # Request GPS data
                gps_response = requests.get(ESP32_GPS_URL, timeout=5)
                gps_data = gps_response.json()

                # Check if 'person' is detected
                if 'person' in df['name'].values:
                    return jsonify({
                        "status": "face_detected",
                        "location": gps_data
                    })
                else:
                    return jsonify({
                        "status": "no_face",
                        "location": gps_data
                    })

        return jsonify({"status": "stream_ended", "location": None})

    except Exception as e:
        print("Error:", e)
        return jsonify({"status": "error", "message": str(e)})

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
