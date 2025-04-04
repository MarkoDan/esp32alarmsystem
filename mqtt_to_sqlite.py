import json
import sqlite3
import time
import paho.mqtt.client as mqtt

# === MQTT Config ===
MQTT_BROKER = "192.168.1.69"
MQTT_PORT = 1883
MQTT_TOPIC = "esp32/alarm"

# === SQLite Setup ===
DB_NAME = "sensor_data.db"

def init_db():
    conn = sqlite3.connect(DB_NAME)
    cursor = conn.cursor()
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS sensor_data (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp TEXT,
            distance REAL,
            temperature REAL,
            humidity REAL,
            alarm_triggered BOOLEAN
        )
    ''')
    conn.commit()
    conn.close()

def insert_data(distance, temperature, humidity, alarm_triggered):
    conn = sqlite3.connect(DB_NAME)
    cursor = conn.cursor()
    cursor.execute('''
        INSERT INTO sensor_data (timestamp, distance, temperature, humidity, alarm_triggered)
        VALUES (?, ?, ?, ?, ?)
    ''', (time.strftime("%Y-%m-%d %H:%M:%S"), distance, temperature, humidity, alarm_triggered))
    conn.commit()
    conn.close()

# === MQTT Callback ===
def on_message(client, userdata, msg):
    try:
        data = json.loads(msg.payload.decode())
        print(f"MQTT Message Received: {data}")
        insert_data(
            data.get("distance"),
            data.get("temperature"),
            data.get("humidity"),
            data.get("alarm_triggered")
        )
    except Exception as e:
        print("Error processing message:", e)

# === Setup MQTT ===
init_db()
client = mqtt.Client()
client.connect(MQTT_BROKER, MQTT_PORT)
client.subscribe(MQTT_TOPIC)
client.on_message = on_message

print(f"Listening to MQTT topic: {MQTT_TOPIC}")
client.loop_forever()
