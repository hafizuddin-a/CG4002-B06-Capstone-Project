# ASL Sign Language Game Visualizer - Unity Project

A **Unity-based AR/XR game visualizer** for the ASL (American Sign Language) Alphabet educational game. This application receives real-time gesture predictions from an Ultra96 FPGA via MQTT and provides interactive visual feedback for learning ASL letters.

## Overview

The visualizer is the end-user-facing component of the CG4002 capstone project. It:

- **Receives predictions** from the Ultra96 FPGA inference engine via MQTT
- **Displays game feedback** based on recognized ASL letters
- **Manages game state** (scores, health, UI updates)
- **Supports multiple platforms**: Mobile (Android/iOS) with AR (ARKit/ARCore), VR (Vuforia, Magic Leap), and desktop
- **Integrates sensors** with on-device testing (letter buttons) and real MQTT client communication

## Project Structure

### Key Directories

- **`Assets/`** — Main game assets and scripts
  - **`Scripts/`** — Core C# game logic and utilities
  - **`Scenes/`** — Game scenes (AR, Vuforia, VR, test scenes)
  - **`M2MqttUnity-master/`** — Third-party MQTT client library
  - **`Common/`** — Shared UI, fonts, materials, prefabs
  - **`AR_Portal/`** — AR-specific prefabs and scripts
  - **`Data/`** — Game configuration and data files
  - **`Resources/`**, **`StreamingAssets/`** — Runtime assets
  - **`Plugins/`** — Native plugins (e.g., for mobile MQTT/TLS)

### Core Scripts

| Script | Purpose |
|--------|---------|
| **`MQTTBridge.cs`** | Main MQTT client that subscribes to `asl/result` topic and decodes predictions |
| **`LetterButtonController.cs`** | Simulates letter predictions via UI buttons (for testing without actual hardware) |
| **`LetterUI.cs`** | Displays the predicted letter on the screen |
| **`MqttClientSimple.cs`** | Simplified MQTT client example |
| **`MQTTManualSender.cs`** | Manual MQTT publisher for testing/debugging |
| **`PlaneMarkerSpawner.cs`** — AR plane detection and letter object spawning |
| **`PlayerInfo.cs`** — Game player state (health, shield, score) |
| **`BillboardToCamera.cs`** — UI billboard behavior (always face camera in 3D) |
| **`CameraModeManager.cs`** — Switches between AR, VR, and standard camera modes |
| **`SettingsController.cs`** — In-game settings and configuration |

### Scenes

- **`Scenes/AR/AR.unity`** — Main AR gameplay scene with plane detection
- **`Scenes/AR/AR_Portal.unity`** — AR Portal visual experience
- **`Scenes/Vuforia.unity`** — Vuforia image target-based AR scene
- **`Scenes/Basic_Face_Filter.unity`** — Face filter AR scene
- **`test.unity`** — Simple test scene for letter button simulation
- **`vrmodeui.unity`** — VR mode UI scene

## Gameplay Mechanics

1. **User performs an ASL letter gesture** using the sensor glove
2. **ESP32 sends sensor data** to the laptop MQTT broker (topic: `glove/data`)
3. **Ultra96 FPGA** receives data, runs inference, and publishes the predicted letter (topic: `asl/result`)
4. **MQTTBridge.cs** receives the prediction and:
   - Displays the letter on screen via `LetterUI.cs`
   - If the prediction is a single character, **destroys matching letter objects** spawned in the scene
   - Awards points or advances the game state
5. **Player can continue** to recognize more letters or complete challenges

## MQTT Integration

### Connection Configuration

The visualizer connects to the laptop MQTT broker as configured in the MQTT client scripts.

**Broker details** (configure in your MQTT client):
- **Host**: Laptop IP address on the phone hotspot (e.g., `192.168.1.100`)
- **Port**: `1883` (default MQTT) or `8883` (TLS)
- **QoS**: Typically QoS=1 for reliable delivery

### MQTT Topics

| Topic | Publisher | Subscriber | Purpose |
|-------|-----------|------------|---------|
| `glove/data` | ESP32 | Ultra96 | Raw sensor data stream |
| `asl/result` | Ultra96 | Unity Visualizer | **Single-character predictions (A-Z)** |
| `visualizer/feedback` | Unity (optional) | — | Game feedback or telemetry |

### MQTT Client Scripts

- **`MQTTBridge.cs`** — Main implementation (commented in current version)
  - Inherits from `M2MqttUnityClient` (from `M2MqttUnity-master`)
  - Subscribes to `asl/result`
  - Calls `DestroyByLetter()` when a letter is recognized

- **`MqttClientSimple.cs`** — Simplified MQTT client wrapper

- **`MQTTManualSender.cs`** — Manual publisher to test sending letters programmatically

### Quick Setup

1. **Enable MQTTBridge.cs**:
   ```csharp
   // In MQTTBridge.cs, uncomment all lines or activate the script in the Inspector
   ```

2. **Assign the MQTT broker connection**:
   - In the Inspector, set the broker host/port to your laptop's IP and `1883`

3. **Test with button simulation**:
   - Use `LetterButtonController.cs` to manually trigger letter destruction
   - Test scenes have buttons A–E that simulate predictions

## Scene Setup & AR Mode

### AR Scene (Main Gameplay)

The `Scenes/AR/AR.unity` scene includes:
- **ARCamera**: ARKit/ARCore camera setup for mobile AR
- **PlaneMarkerSpawner.cs**: Detects planes and spawns letter objects
- **MQTTBridge**: Receives predictions and triggers letter destruction
- **GameUI**: Displays current letter, score, health

### Testing without AR

For desktop/editor testing:
1. Open `test.unity` scene
2. Use the letter buttons (A, B, C, D, E) in the UI
3. Watch objects spawn and destroy based on your selections
4. Alternatively, use `MQTTManualSender.cs` to send letters via MQTT

### VR/Vuforia Scenes

- **`Vuforia.unity`** — Image-based AR with Vuforia
- **`vrmodeui.unity`** — VR UI mode for headset-based gameplay
- Requires Vuforia Engine package and a cloud database for image targets

## Building & Running

### Prerequisites

- **Unity 2023.2+** (or version matching the project)
- **Mobile device** (Android/iOS) with:
  - ARKit (iOS) or ARCore (Android)
  - Network access to laptop MQTT broker (same phone hotspot)
- **Vuforia account** (optional, for image target AR)
- **Development SDK**: Android SDK (for Android), Xcode (for iOS)

### Build Steps

1. **Open the project** in Unity:
   ```bash
   # Clone or open SW Visualizer (new) folder in Unity Hub
   ```

2. **Select your target scene** (e.g., `Scenes/AR/AR.unity`)

3. **Configure build settings**:
   - **File → Build Settings**
   - Select platform: **Android** or **iOS**
   - Scene selection: Include `AR.unity` (or your desired scene)

4. **Configure MQTT broker**:
   - In the scene, find the MQTT client GameObject
   - Set **Broker Host** to your laptop IP (e.g., `192.168.1.100`)
   - Set **Broker Port** to `1883`

5. **Build and run**:
   ```bash
   # Android
   File → Build and Run (Android device must be connected)
   
   # iOS
   File → Build → Xcode, then build in Xcode
   ```

### Testing on Editor

1. Open `test.unity` scene
2. Press **Play** in the Editor
3. Click the letter buttons (A–E) to simulate predictions
4. Alternatively, run a local MQTT broker and use `MQTTBridge.cs` to receive real messages

## Key Dependencies

| Package | Version | Purpose |
|---------|---------|---------|
| **XR Interaction Toolkit** | 3.1.1 | XR (VR/AR) interaction |
| **ARKit XR Plugin** | 6.1.0 | iOS AR support |
| **ARCore XR Plugin** | 6.1.0 | Android AR support |
| **AR Foundation** | 6.1.0 | Cross-platform AR API |
| **Vuforia Engine** | 11.4.4 | Image-based AR (optional) |
| **Magic Leap SDK** | 2.6.0 | Magic Leap device support (optional) |
| **TextMesh Pro** | Built-in | Advanced text rendering |
| **Visual Scripting** | 1.9.8 | Node-based logic (optional) |

## Network & Connectivity

### Phone Hotspot Setup

1. **ESP32** connects to phone hotspot (SSID and password configured in Arduino firmware)
2. **Laptop** also connects to the same phone hotspot
3. **Laptop runs MQTT broker** (e.g., Mosquitto on port 1883)
4. **Ultra96** accesses the broker via SSH reverse tunnel (see main README)
5. **Unity app** connects to the laptop broker using the laptop's hotspot IP

### Finding Your Laptop IP on Phone Hotspot

- **Android**: Settings → Hotspot → Connected devices (see laptop IP)
- **iPhone**: Settings → Personal Hotspot → scroll down to see connected devices
- **Command line**:
  ```bash
  # On laptop
  ipconfig (Windows)
  ifconfig (macOS/Linux)
  # Look for the IP assigned on the hotspot interface
  ```

## Troubleshooting

### MQTT Connection Issues

- **"Cannot connect to broker"**: Verify laptop IP address and firewall allows port 1883
- **"Topics not received"**: Check Ultra96 is publishing to `asl/result`
- **"TLS connection failed"**: Use `mqtt_client_tls.py` and ensure certificates are valid

### AR Issues

- **"No plane detected"**: Move device slowly, expose real-world surfaces (walls, floors)
- **"Letters not spawning"**: Confirm `PlaneMarkerSpawner.cs` is active and plane detection works

### Build Errors

- **"Missing Vuforia or AR Foundation"**: Re-import packages in `Packages/manifest.json`
- **"M2MqttUnity not found"**: Ensure `Assets/M2MqttUnity-master/` is not excluded

## Extending the Game

### Adding New Letters

1. Create a new letter prefab (duplicate existing letter object)
2. Update `PlaneMarkerSpawner.cs` to spawn the new letter
3. Update Ultra96 inference model to output the new letter
4. Test with `LetterButtonController.cs` button

### Custom UI/Themes

- Modify materials in `Assets/Common/Materials/`
- Adjust colors and fonts in `Assets/TextMesh Pro/`
- Update prefabs in `Assets/Common/Prefabs/`

### Multiplayer / Scoring

- Extend `PlayerInfo.cs` for multi-player state
- Add new UI panels in `Assets/Common/`
- Broadcast game events via MQTT to other clients

## Performance Optimization

- **Mobile**: Disable visual effects on lower-end devices; use simpler models for spawned objects
- **AR**: Optimize plane detection frequency; reduce letter object polygon count
- **Network**: Use QoS=0 for non-critical feedback; throttle MQTT publish rate on Ultra96

## Documentation

- **`Assets/M2MqttUnity-master/README.md`** — MQTT client library documentation
- **Main project `README.md`** — System-wide architecture and data flow
- **`../Comms/mqtt_client.py`** — Example MQTT client scripts and topic conventions

## Contributors

**CG4002 Capstone Project - B06 Group**

## License

Educational project. All rights reserved.

---

**Note**: This visualizer is designed to run on mobile devices with AR capability and connect to a laptop MQTT broker. Ensure your device and network are properly configured for real-time gesture recognition and game feedback.
