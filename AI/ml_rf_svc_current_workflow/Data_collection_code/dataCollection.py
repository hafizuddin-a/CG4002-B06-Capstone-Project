import serial
import threading
import sys
import string

# Open serial connection
com_port = "COM3"
baud_rate = 115200

# Flag to control thread shutdown
running = True
current_label = 'unlabeled'
mode = "manual"

# Function to read keyboard input and send to ESP32
def keyboard_listener():
    global current_label
    global mode
    print("💻 Type and press ENTER to send input to ESP32 (empty input = ENTER key):")
    while running:
        try:
            user_input = sys.stdin.readline()
            if user_input.startswith("auto"):
                mode = "auto"
                current_label = "J"
                print(f"Mode changed to: auto. Current_label: A. Goes A-J everytime you press enter from now on")
            elif user_input.startswith("manual"):
                mode = "manual"
                current_label = "unlabeled"
                print(f"Mode changed to: manual. Current_label: unlabeled")
            elif user_input.strip():
                current_label = user_input.strip()
                print(f"Label changed to: {current_label}")
            else:
                if mode == 'auto':
                    current_label = chr((ord(current_label) - ord('A') + 1) % 10 + ord("A"))
                print(f"current_label: {current_label}")
                ser.write(b'\n')

        except Exception as e:
            print(f"⚠️ Keyboard thread error: {e}")
            break

try:
    ser = serial.Serial(com_port, baud_rate, timeout=1)
    
    # Start the keyboard listener in a separate daemon thread
    thread = threading.Thread(target=keyboard_listener, daemon=True)
    thread.start()

    with open('glove_data_22.csv', 'w') as f:
        # Write header
        header = ""
        for i in range(1, 101):
            header += f"roll_{i}, pitch_{i}, yaw_{i}, ax_{i}, ay_{i}, az_{i}, t0_{i}, t1{i}, f0_{i}, f1_{i}, f2_{i}, f3_{i}, "
        header += "LABEL"
        f.write(header.rstrip(", ") + '\n')

        while True:
            line = ser.readline().decode(errors='ignore').strip()
            values = line.split(',')

            if len(values) == 1200:
                print(f"✔️ Received 100-step row (first 12 values): {line.split(",")[:13]}")
                f.write(','.join(values) + f",{current_label}\n")
                f.flush()
            elif line != "":
                print(f"⚠️ Received non-data message: {line}")

except KeyboardInterrupt:
    print("\n🛑 Ctrl+C detected. Shutting down...")

except Exception as e:
    print(f"An error occurred: {e}. Shutting down...")

finally:
    running = False
    ser.close()
    print(f"✅ Serial port {com_port} closed.")