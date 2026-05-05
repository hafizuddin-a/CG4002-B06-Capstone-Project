import paho.mqtt.client as mqtt
import time
import queue

clientId = "Laptop"
port = 1883
host = "localhost"
messageQueue = queue.Queue(maxsize = 200)

def on_subscribe(client, userdata, mid, reason_code_list, properties):
    # Since we subscribed only for a single channel, reason_code_list contains
    # a single entry
    if reason_code_list[0].is_failure:
        print(f"Broker rejected you subscription: {reason_code_list[0]}")
    else:
        print(f"Broker granted the following QoS: {reason_code_list[0].value}")

def on_unsubscribe(client, userdata, mid, reason_code_list, properties):
    # Be careful, the reason_code_list is only present in MQTTv5.
    # In MQTTv3 it will always be empty
    if len(reason_code_list) == 0 or not reason_code_list[0].is_failure:
        print("unsubscribe succeeded (if SUBACK is received in MQTTv3 it success)")
    else:
        print(f"Broker replied with failure: {reason_code_list[0]}")
    client.disconnect()

def on_message(client, userdata, message):
    # userdata is the structure we choose to provide, here it's a list()
    # userdata.put(message.payload.decode())
    # print(f"Received the following message: {message.payload.decode()}")
    # We only want to process 10 messages
    # if len(userdata) >= 10:
    #     client.unsubscribe("esp/pub")
    result = message.payload.decode()
    mqttc.publish("asl/result", result, qos=1)

def on_connect(client, userdata, flags, reason_code, properties):
    if reason_code.is_failure:
        print(f"Failed to connect: {reason_code}. loop_forever() will retry connection")
    else:
        # we should always subscribe from on_connect callback to be sure
        # our subscribed is persisted across reconnections.
        client.subscribe("esp/pub")

# Function to process data
def process_data(dataQueue):
    try:
        data = dataQueue.get_nowait()
    except queue.Empty:
        return "no data"
    # Carry out AI inference based on data
    # print(data)
    # result = 'A' if data == "my message" else 'B'
    # return result
    return data

# Set up MQTT client
mqttc = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
mqttc.on_connect = on_connect
mqttc.on_message = on_message
mqttc.on_subscribe = on_subscribe
mqttc.on_unsubscribe = on_unsubscribe

mqttc.user_data_set(messageQueue)
mqttc.connect(host, port)
mqttc.loop_start()
# print(f"Received the following message: {mqttc.user_data_get()}")

# Main function
# try:
#     while True:
#         # time.sleep(1)  # Main program loop
#         # print("Main code execution...")
#         dataQueue = mqttc.user_data_get()
#         if dataQueue.empty():
#             print(f"\rNo data received yet", end="", flush=True)
#         else:
#             print(f"\r{dataQueue.qsize()}", end="", flush=True)
#         # result = process_data(dataQueue)
#         # if (result != "no data"):
#         #     mqttc.publish("asl/result", result, qos=1)
        
# except KeyboardInterrupt:
#     print("\nExiting...")
#     # print(dataQueue)
# finally:
#     mqttc.loop_stop()
#     mqttc.disconnect()
try:
    while (True):
        print(f"\rRunning...", end="", flush=True)
except KeyboardInterrupt:
    print("\nExiting...")
    # print(dataQueue)
finally:
    mqttc.loop_stop()
    mqttc.disconnect()