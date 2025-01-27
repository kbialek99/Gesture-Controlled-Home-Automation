# Use a base image with Python 3.10
FROM python:3.10-slim

# Set the working directory in the container
WORKDIR /app

# Copy the current directory contents (your app) into the container
COPY . /app

# Install the necessary Python packages
RUN pip install --no-cache-dir -r requirements.txt

# Install system dependencies (for OpenCV, Mediapipe, etc.)
RUN apt-get update && apt-get install -y \
    libglib2.0-0 \
    libsm6 \
    libxext6 \
    libxrender-dev \
    libgl1-mesa-glx  # This is the package that provides libGL.so.1

# Expose the port that the Flask app will run on
EXPOSE 80

# Command to run your app
CMD ["python", "server.py"]
