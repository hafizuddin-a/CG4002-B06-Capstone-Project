import paho.mqtt.client as mqtt
import time
import queue
import sys
import threading
import ast
import numpy as np

# MQTT Configuration
clientId = "DataCollector"
port = 8883
host = "localhost"
caCertPath = "./ca.crt"
clientCertPath = "./U96_client.crt"
clientKeyPath = "./U96_client.key"

# Data collection parameters
DATA_TIMESTEPS = 100
messageQueue = queue.Queue(maxsize=100)
current_label = 'unlabeled'
mode = "manual"
running = True

file_path = "data/glove_data_mqtt_1.csv"

# ==================================================
# MQTT Callbacks
# ==================================================
def on_connect(client, userdata, flags, reason_code, properties):
    if reason_code.is_failure:
        print(f"Failed to connect: {reason_code}")
    else:
        print("✅ Connected to MQTT broker")
        client.subscribe("glove/data", qos=0)
        client.subscribe("glove/status", qos=1)
        

def on_message(client, userdata, message):
    if (message.topic == "glove/status"):
        print(message.payload.decode())
    elif (message.topic == "glove/data"):
        """Store incoming sensor readings"""
        try:
            # Parse the list: [roll, pitch, yaw, ax, ay, az, t0, t1, f0, f1, f2]
            data_list = ast.literal_eval(message.payload.decode())

            # Verify we have exactly 12 values
            if len(data_list) == 12:
                if not userdata.full():
                    userdata.put(data_list)
                #print(data_list)
            else:
                print(f"Expected 12 values, got {len(data_list)}")

        except Exception as e:
            print(f"Error parsing message: {e}")
            print(f"   Raw message: {message.payload.decode()}")

def on_subscribe(client, userdata, mid, reason_code_list, properties):
    if reason_code_list[0].is_failure:
        print(f"Broker rejected subscription: {reason_code_list[0]}")
    else:
        print(f"Subscribed with QoS: {reason_code_list[0].value}")

# ==================================================
# Keyboard Listener
# ==================================================
def keyboard_listener():
    global current_label
    global mode
    labels = ['REST','C']
    #labels = ['REST','A','REST','B','REST','C','REST','D','REST','E','REST','F','REST','G','REST','H','REST','I','REST','J']
    current_index = 0

    while running:
        try:
            user_input = sys.stdin.readline().strip()

            if user_input.startswith("auto"):
                mode = "auto"
                current_index = 0
                current_label = labels[current_index]
                print(f"📋 Mode: auto | Label: {current_label}")

            elif user_input.startswith("manual"):
                mode = "manual"
                current_label = 'unlabeled'
                print(f"📋 Mode: manual | Label: {current_label}")

            elif user_input:
                # Set custom label
                current_label = user_input
                print(f"📋 Label set to: {current_label}")

            else:
                # ENTER pressed - trigger data collection
                collect_sample()

                if mode == 'auto':
                    # increment index and wrap around
                    current_index = (current_index + 1) % len(labels)
                    current_label = labels[current_index]
                    print(f"📋 Next label: {current_label}")

        except Exception as e:
            print(f"⚠️ Keyboard thread error: {e}")
            break


def collect_sample():
    """Collect 100 timesteps from the queue and write to CSV"""
    # Clear stale data
    flushed = 0
    while not messageQueue.empty():
        try:
            messageQueue.get_nowait()
            flushed += 1
        except queue.Empty:
            break
    
    if flushed > 0:
        print(f"🗑️  Flushed {flushed} old messages")
    
    print(f"⏳ Collecting {DATA_TIMESTEPS} timesteps in real-time...")
    
    collected = []
    start_time = time.time()
    
    while len(collected) < DATA_TIMESTEPS:
        try:
            if time.time() - start_time > 5:
                print(f"⚠️ Timeout - only collected {len(collected)} timesteps")
                return
                
            row = messageQueue.get(timeout=0.1)
            collected.append(row)
            
            # Progress indicator
            if len(collected) % 25 == 0:
                print(f"  📊 {len(collected)}/{DATA_TIMESTEPS}")
             
        except queue.Empty:
            continue
    
    duration = time.time() - start_time
    rate = DATA_TIMESTEPS / duration
    print(f"✅ Collected {DATA_TIMESTEPS} samples in {duration:.2f}s ({rate:.1f} Hz)")
    
    # Flatten the 100 timesteps into a single CSV row
    flattened = []
    for timestep in collected:
        flattened.extend(timestep)  # Each timestep is already a list of 13 values
    
    # Verify we have exactly 1300 values (100 timesteps × 13 values)
    if len(flattened) != 1200:
        print(f"⚠️ Expected 1300 values, got {len(flattened)}")
        return
    
    # Write to CSV
    with open('data/glove_data_mqtt_17.csv', 'a') as f:
        csv_line = ','.join(map(str, flattened)) + f",{current_label}\n"
        f.write(csv_line)
        f.flush()
    
    np_arr = np.array(collected)
    
    print(f"✔️ Saved 100-timestep sample with label: {current_label}")
    print(f"   Median timestep values: {np.median(np_arr, axis=0)}")  # Show first 6 values

# ==================================================
# Main
# ==================================================
def main():
    global running
    
    # Initialize CSV file with header
    print("📁 Initializing CSV file...")
    with open('data/glove_data_mqtt_17.csv', 'w') as f:
        header = ""
        for i in range(1, DATA_TIMESTEPS + 1):
            header += f"roll_{i},pitch_{i},yaw_{i},ax_{i},ay_{i},az_{i},t0_{i},t1_{i},f0_{i},f1_{i},f2_{i},f3_{i},"
        header += "LABEL\n"
        f.write(header)
    
    print("✅ CSV initialized: glove_data_mqtt.csv")
    
    # Set up MQTT client
    mqttc = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=clientId)
    mqttc.tls_set(
        ca_certs=caCertPath,
        certfile=clientCertPath,
        keyfile=clientKeyPath,
        tls_version=mqtt.ssl.PROTOCOL_TLSv1_2
    )
    mqttc.tls_insecure_set(True)
    mqttc.on_connect = on_connect
    mqttc.on_message = on_message
    mqttc.on_subscribe = on_subscribe
    mqttc.user_data_set(messageQueue)
    
    print(f"🔌 Connecting to MQTT broker at {host}:{port}...")
    mqttc.connect(host, port, keepalive=60)
    mqttc.loop_start()
    
    # Start keyboard listener thread
    kb_thread = threading.Thread(target=keyboard_listener, daemon=True)
    kb_thread.start()
    
    try:
        print("\n🚀 Ready for data _sample collection!")
        while True:
            time.sleep(0.1)
            
    except KeyboardInterrupt:
        print("\n🛑 Ctrl+C detected. Shutting down...")
        
    finally:
        running = False
        mqttc.loop_stop()
        mqttc.disconnect()
        print("✅ Disconnected from MQTT broker")

if __name__ == "__main__":
    main()