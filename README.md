
# ASL Sign Language Alphabet Learning Game - CG4002 Capstone Project

An educational game designed to teach **American Sign Language (ASL) Alphabet** using a sensor glove, FPGA-accelerated AI inference, and a Unity visualizer. The system combines embedded sensors, an ESP32 microcontroller, an Ultra96 FPGA for inference, and a Unity-based game for interactive feedback.

## Project Overview

This capstone project implements an end-to-end system for recognizing ASL alphabet signs using a custom glove and FPGA-accelerated AI inference:

1. **Gesture Capture**: A glove equipped with flex sensors, IMU (Inertial Measurement Unit), and contact sensors captures hand and finger movements.
2. **Data Transport**: The ESP32 sends sensor data over Wi-Fi to a local MQTT broker running on a laptop (phone hotspot network).
3. **Edge Inference**: The Ultra96 FPGA subscribes to the broker (via an SSH reverse tunnel) and runs the AI inference using HLS4ML/Vitis code.
4. **Visualization**: The Ultra96 publishes predicted labels back to the laptop broker; the Unity visualizer subscribes to the broker to receive predictions for game logic.

## System Architecture

```
[Sensor Glove] 
      ↓ (I2C/ADC)
[FireBeetle ESP32]
      ↓ (Wi‑Fi via phone hotspot)
[Laptop running MQTT broker]
      ↑          ↑
      |          |
      |    (Unity Visualizer subscribes)
      |
   (SSH reverse tunnel)
      |
[Ultra96 FPGA] (runs AI inference, acts as MQTT client via tunnel)
      ↓ (Prediction published to broker)
[Unity Game Visualizer] (subscribes to laptop broker)
```

## Communications (New)

- **Network topology**: The ESP32 and the laptop are on the same phone mobile hotspot. The laptop runs an MQTT broker locally (e.g., Mosquitto).
- **Broker**: MQTT broker runs locally on the laptop. Both ESP32 and Unity visualizer connect to this broker.
- **Ultra96 access**: The Ultra96 is behind a firewall and only exposes SSH. The laptop establishes an SSH reverse tunnel to allow the Ultra96 to access the laptop's MQTT broker as if it were on the same local network.

   - Typical reverse SSH tunnel command run from the Ultra96 (SSH client) to the laptop (SSH server):

   ```bash
   # On Ultra96 (example):
   ssh -R 1883:localhost:1883 user@laptop.example.com -N
   # This forwards the laptop's port 1883 to the Ultra96, allowing the Ultra96 to reach the laptop broker.
   ```

   - Alternatively, create the tunnel from the laptop to Ultra96 with remote port forwarding depending on network permissions. Adjust ports and hosts as needed.

- **Data flow (communications)**:
   1. ESP32 publishes glove sensor topics to the laptop MQTT broker.
   2. Ultra96 (through the reverse SSH tunnel) subscribes to the glove topics from the laptop broker and receives sensor data.
   3. Ultra96 performs AI inference and publishes predicted labels/results back to the laptop broker.
   4. Unity visualizer subscribes to prediction topics on the laptop broker and updates the game state.

- **Scripts and examples**: See `Comms/` for MQTT client examples:
   - `Comms/mqtt_client.py` — example Python MQTT client used by visualizer or laptop-side tools.
   - `Comms/mqtt_client_tls.py` — TLS-enabled MQTT client example.
   - `Comms/mqtt_client copy.py` — alternate client used during development.

## Project Structure

### 📡 Hardware (`/Hardware`)
- **FireBeelte-ESP32.ino**: Main firmware for the ESP32 microcontroller that reads from flex sensors, IMU, and contact sensors.
- **FireBeetle-ESP32_CollectData.ino**: Data collection firmware for gathering training datasets from sensor glove.

### 🤖 AI & ML (`/AI`)
- **hls4ml_WithSampleDataInput/**: Python-based model training and HLS conversion
   - `Model.ipynb`: Jupyter notebook for training the neural network
   - Sample sensor data CSV files for testing and validation
- **Vitis_WithSampleDataInput/**: High-Level Synthesis (HLS) implementation
   - `firmware/myproject.cpp`: C++ HLS implementation of the neural network
   - `myproject_test.cpp`: Test bench for validation
   - `hls4ml_config.yml`: Configuration for HLS4ML code generation
   - Pre-generated weights and parameters for inference

### 🎮 Software Visualizer (`/SW Visualizer`)
- **Unity-based game environment** for interactive ASL learning
- **MQTT Integration**: Receives prediction results from FPGA via the laptop MQTT broker
- **AR Portal**: AR-based visualization components
- **Game Controllers**: Letter-based game mechanics for ASL alphabet learning

## Workflow (updated)

### 1. Hardware Setup
1. Assemble the sensor glove with:
    - Flex sensors on each finger
    - IMU for hand orientation/motion
    - Contact sensors for gesture detection
2. Configure ESP32 with appropriate firmware
3. Connect ESP32 and laptop to a phone mobile hotspot (same network)

### 2. Communications Setup
1. Start a local MQTT broker on the laptop (e.g., Mosquitto on port `1883`).
2. Verify the ESP32 can connect and publish sample data to the broker.
3. Set up SSH reverse tunnel so Ultra96 can reach the laptop broker (see examples above).
4. Confirm Ultra96 can subscribe to glove topics through the tunnel and publish predictions.

### 3. AI Model Development
1. Collect training data using `FireBeetle-ESP32_CollectData.ino`.
2. Train neural network in `hls4ml_WithSampleDataInput/Model.ipynb`.
3. Convert trained model to HLS4ML for FPGA deployment.

### 4. FPGA Implementation & Deployment
1. Use Vitis HLS to synthesize `firmware/myproject.cpp` and run `myproject_test.cpp`.
2. Package the HLS IP, integrate in Vivado, and generate a bitstream for Ultra96.
3. Deploy the bitstream and runtime on Ultra96. Ensure SSH access is available for tunnel setup.

### 5. Game Integration
1. Configure Unity visualizer's MQTT client to connect to the laptop broker.
2. Verify the visualizer receives prediction messages and updates game logic.

## Key Technologies

| Component | Technology |
|-----------|-----------|
| **Microcontroller** | FireBeetle ESP32 |
| **FPGA Board** | Ultra96 (Xilinx Zynq UltraScale+) |
| **ML Framework** | HLS4ML (High-Level Synthesis for ML) |
| **HLS Synthesis** | Vitis HLS 2025.1 |
| **FPGA Design** | Vivado |
| **Game Engine** | Unity 3D |
| **Communication** | MQTT, SSH reverse tunnelling |

## Features

✅ **Real-time Gesture Recognition**: Dual flex sensors and IMU-based hand gesture capture  
✅ **FPGA-Accelerated Inference**: Hardware-optimized neural network for low-latency predictions  
✅ **Interactive Game Interface**: Unity-based AR/3D visualization for engaging learning experience  
✅ **Wireless Connectivity**: ESP32 for wireless sensor data transmission  
✅ **Flexible Comms**: Laptop MQTT broker and SSH reverse tunnelling allow Ultra96 operation behind firewalls

## Getting Started

### Prerequisites
- **Hardware**: FireBeetle ESP32, Ultra96 FPGA Board, Flex Sensors, IMU, Contact Sensors
- **Software**: 
   - Mosquitto (or other MQTT broker) on laptop
   - Vitis HLS 2025.1 (Windows x64)
   - Vivado Design Suite
   - PYNQ Framework
   - Unity 3D
   - Python 3.x with TensorFlow/Keras

### Quick Start — Communications & Run Order

1. Start the MQTT broker on the laptop (example using Mosquitto):

```bash
# On laptop (Windows/Linux/macOS):
mosquitto -v
```

2. Connect the ESP32 to the laptop broker (configure SSID/password to the phone hotspot and broker IP).

3. On Ultra96, create the reverse SSH tunnel to the laptop so Ultra96 can reach the broker (example shown earlier).

4. Start the Ultra96 inference script (it will subscribe to glove topics and publish predictions).

5. Start the Unity visualizer and confirm it connects to the laptop broker and receives prediction messages.

## Dataset

The project uses sensor-based gesture data compatible with the [Glove Gesture Dataset from Kaggle](https://www.kaggle.com/datasets/mouadfiali/sensor-based-american-sign-language-recognition).

## Documentation

- `AI/README.md` - AI workflow and model conversion pipeline
- `AI/Vitis_WithSampleDataInput/README.md` - HLS synthesis detailed guide
- `Comms/` - MQTT client examples and helper scripts
- `SW Visualizer/readme.md` - Visualizer details (Unity project)

## Contributors

**CG4002 Capstone Project - B06 Group**

## License

This project is part of an educational capstone program.

## Acknowledgments

- Kaggle ASL Recognition Dataset
- Xilinx HLS4ML Framework
- Unity Game Engine
- PYNQ Project

---

**Note**: This README documents the communication topology where a laptop-hosted MQTT broker and SSH reverse tunnelling enable an Ultra96 behind a firewall to participate as an MQTT client. Adjust hostnames, ports, and authentication to match your local network and security requirements.
