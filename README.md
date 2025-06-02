# TUBITAK - FALL DETECTION SYSTEM WITH DEEP LEARNING METHODS
In this TÜBİTAK-supported project, I was responsible for the hardware integration and software development of the ESP32-CAM and accelerometer components used in the fall detection system. In the system, when a sudden movement or fall is detected through accelerometer data, the ESP32-CAM module is triggered to capture an image of the environment. The captured image is then sent to an AI model based on YOLO and Convolutional Neural Networks (CNNs) for fall detection

# PROJECT POSTER
![DERİN ÖĞRENME YÖNTEMLERİ İLE DÜŞME TESPİT SİSTEMİ (1)](https://github.com/user-attachments/assets/a5c3e7a3-da21-4247-a5c1-070ee4bacc81)


# MPU6050 ACCELEROMETER
An MPU6050 accelerometer is placed on the user's body to continuously monitor real-time acceleration.
When the calculated acceleration exceeds a predefined threshold, a connected camera module is activated.

## Features
- Acceleration is calculated using a combination of the x, y, and z axes.  
- Internet connection details required for the accelerometer are provided using SoftAP (Software Access Point) configuration.

![image](https://github.com/user-attachments/assets/76be7f1c-46e2-4679-b5df-275957e8d798)
![image](https://github.com/user-attachments/assets/a2e1b691-653c-4891-bfb6-767a5848e172)

# ESP32-CAM
Based on commands received from the accelerometer, the ESP32-CAM captures the surrounding environment. The image is then sent to an AI model to determine whether a fall has occurred.

## Features
- **SoftAP =>** Provides the necessary internet connection setup for the ESP32-CAM using Software Access Point mode.  
- **Video Streaming =>** Allows users to monitor their elderly relatives in real-time through live video streaming whenever they wish.  
- **Static IP =>** Ensures consistent access to the ESP32-CAM by using a static IP address, avoiding issues caused by changing IPs across different networks.  

![image](https://github.com/user-attachments/assets/5f659469-6da1-42dc-9751-83aa2f74594c)
![image](https://github.com/user-attachments/assets/5567e449-6834-433c-a3c1-bb3a4de8e127)

