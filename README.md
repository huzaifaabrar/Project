# Dual-Rate Audio Processing and Fire Alarm Detection using ESP32

This repository contains the complete implementation of a real-time fire alarm detection system based on audio spectral analysis using an ESP32 microcontroller and FreeRTOS. The system captures audio via an I2S MEMS microphone, performs FFT-based frequency analysis using a dual-rate running-average approach, and sends real-time alerts to connected web clients using WebSocket communication.

---

## 📌 Features

- Real-time audio acquisition using I2S + DMA  
- FFT-based spectral analysis optimized for embedded systems  
- Running-average FFT for noise suppression and frequency stability  
- Dual-rate processing (short and long analysis windows)  
- Reliable detection of fire alarm tones (2750–3250 Hz)  
- FreeRTOS-based multitasking with task prioritization and core pinning  
- Web-based real-time notification using WebSocket  
- UART logging for debugging and verification  

---

## 🧠 System Overview

The system continuously captures audio samples from an I2S MEMS microphone. Audio data is processed by FFT tasks running under FreeRTOS to detect persistent narrow-band frequency components typical of fire alarms. Upon detection, an event is generated and transmitted to connected web clients in real time.

### High-Level Data Flow

I2S Microphone -> I2S DMA Buffers -> FFT Processing Task -> Fire Alarm Event Queue -> WebSocket / UART Notification


---

## 🛠 Hardware Requirements

- ESP32 Development Board  
- I2S MEMS Microphone (e.g., INMP441)  
- Wi-Fi Network  
- USB Power Supply  

---

## 💻 Software Stack

- **ESP-IDF**  
- **FreeRTOS**  
- **ESP32 I2S Driver**  
- **FFT Library**  
- **WebSocket Server**  
- **HTML / JavaScript (Web Client)**  

---

## 🧩 Software Architecture

### FreeRTOS Tasks

| Task Name | Description | Priority | Core |
|---------|-------------|----------|------|
| I2S Reader Task | Captures audio samples via DMA | 5 | Core 0 |
| FFT Processing Task | Windowing, FFT, detection logic | 4 | Core 0 |
| Web Notification Task | Sends events via UART/WebSocket | 3 | Core 1 |
| Wi-Fi Task | Manages network connectivity | Default | Core 1 |

### Inter-Task Communication

- Audio buffer queue (I2S → FFT)  
- Fire alarm event queue (FFT → Web task)  

---

## 📈 FFT Processing Strategy

Instead of using a large sliding or block-based FFT, this project implements a **running-average FFT** approach:

- Short FFT frames are processed continuously  
- FFT magnitudes are averaged over time  
- Reduced memory usage and lower latency  
- Improved detection stability  
- Effective suppression of transient noise  

### Dual-Rate Analysis

- **Short window (1 s):** Fast detection, lower frequency resolution  
- **Long window (4 s):** Slower response, higher frequency resolution  

---

## 🔍 Detection Algorithm

1. Normalize audio samples  
2. Apply Hamming window  
3. Compute FFT  
4. Convert magnitude to decibels (dB)  
5. Monitor frequency bins in the 2750–3250 Hz range  
6. Apply persistence counter to avoid false detections  
7. Generate fire alarm event upon confirmation  

---

## 🌐 Web Interface

- ESP32 hosts an HTTP server  
- Accessible via assigned IP address  
- WebSocket provides real-time updates  
- Logs displayed with timestamps  
- No page refresh required  

Example access:
http://ESP32-IP-ADDRESS


---

## 📁 Repository Structure

Project/  
├── main/  
│ ├── audio_i2s.c  
│ ├── fft_processing.c  
│ ├── wifi_comm.c  
│ ├── websocket_server.c  
│ └── main.c  
├── include/  
│ ├── audio_i2s.h  
│ ├── fft_processing.h  
│ ├── wifi_comm.h  
│ └── websocket_server.h  
├── web/  
│ └── index.html  
├── CMakeLists.txt  
├── sdkconfig  
└── README.md  


---

## 🚀 How to Build and Flash

1. Install ESP-IDF  
2. Clone the repository:
   ```bash
   git clone https://github.com/huzaifaabrar/Project.git
   cd Project
3. Configure the project:
   ```bash
   idf.py menuconfig
4. Build and flash:
   ```bash
   idf.py build flash monitor

---

## ✅ Results

- Reliable detection of fire alarm tones
- Low-latency real-time alerts
- Stable operation under multitasking
- Efficient use of memory and CPU resources

---

## 🔮 Future Enhancements

- Support for multiple WebSocket clients
- Graphical FFT visualization on web UI
- Audio signature-based classification
- Detection of additional acoustic events
- Power optimization for battery operation


---
