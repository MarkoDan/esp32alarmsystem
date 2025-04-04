#include <Arduino.h>
#include <DHT.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <FS.h>
#include <SPIFFS.h>

// WiFi credentials
const char* ssid = "ALHN-179D";
const char* password = "qkDUaRAJj6";

// MQTT configuration
const char* mqtt_server = "locahost";
const int mqtt_port = 1883;
const char* mqtt_topic = "esp32/alarm";

WiFiClient espClient;
PubSubClient client(espClient);

// Pin definitions
#define DHTPIN 4
#define DHTTYPE DHT11
#define TRIG_PIN 15
#define ECHO_PIN 2
#define BUZZER_PIN 23

// LED bar graph pins
const int ledPins[] = {13, 12, 14, 27, 26, 25, 33, 32};
const int numLEDs = sizeof(ledPins) / sizeof(ledPins[0]);

// Danger threshold Afstand til alarm aktivation 80cm
const int dangerDistance = 80;

DHT dht(DHTPIN, DHTTYPE);

void setup_wifi();
void try_reconnect_mqtt();

void setup() {
  Serial.begin(115200);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  for (int i = 0; i < numLEDs; i++) {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW);
  }

  dht.begin();
  Serial.println("Alarm system with LED bar graph active.");

  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS failed to mount.");
  } else {
    Serial.println("SPIFFS ready.");
  }

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
}

void setup_wifi() {
  delay(10);
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected. IP: " + WiFi.localIP().toString());
}

// Attempt reconnect to MQTT broker
void try_reconnect_mqtt() {
  if (!client.connected() && WiFi.status() == WL_CONNECTED) {
    Serial.print("Trying MQTT reconnect...");
    if (client.connect("ESP32Client")) {
      Serial.println("connected");
    } else {
      Serial.print(" failed, rc=");
      Serial.print(client.state());
    }
  }
}

void loop() {
  // Measure distance using ultrasonic sensor
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH);
  float distance = duration * 0.034 / 2;

  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.println(" cm");

  // Trigger buzzer and LED animation if object is close
  if (distance < dangerDistance && distance > 0) {
    digitalWrite(BUZZER_PIN, HIGH);
    for (int i = 0; i < numLEDs; i++) {
      digitalWrite(ledPins[i], HIGH);
      delay(30);
      digitalWrite(ledPins[i], LOW);
    }
  } else {
    digitalWrite(BUZZER_PIN, LOW);
    for (int i = 0; i < numLEDs; i++) {
      digitalWrite(ledPins[i], LOW);
    }
  }

  // Read temperature and humidity from DHT11
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();

  if (!isnan(temp) && !isnan(hum)) {
    Serial.print("Temperature: ");
    Serial.print(temp);
    Serial.print(" °C, Humidity: ");
    Serial.print(hum);
    Serial.println(" %");
  } else {
    Serial.println("Failed to read from DHT sensor!");
  }

  // Keep MQTT alive if WiFi is connected
  if (WiFi.status() == WL_CONNECTED) {
    try_reconnect_mqtt();
    client.loop();
  } else {
    Serial.println("WiFi disconnected, cannot connect to MQTT.");
  }

  bool alarm_triggered = (distance < dangerDistance && distance > 0);

  // Create JSON payload
  String payload = "{";
  payload += "\"distance\":" + String(distance, 2) + ",";
  payload += "\"temperature\":" + String(temp, 2) + ",";
  payload += "\"humidity\":" + String(hum, 2) + ",";
  payload += "\"alarm_triggered\":" + String(alarm_triggered ? "true" : "false");
  payload += "}";

  Serial.print("Sending MQTT: ");
  Serial.println(payload);

  // Publish to MQTT or cache to SPIFFS
  if (client.connected()) {
    client.publish(mqtt_topic, payload.c_str());
    Serial.println("MQTT publish success");

    // Send cached messages from SPIFFS
    File cacheFile = SPIFFS.open("/cache.txt", "r");
    if (cacheFile) {
      while (cacheFile.available()) {
        String cached = cacheFile.readStringUntil('\n');
        client.publish(mqtt_topic, cached.c_str());
        Serial.println("Sent cached: " + cached);
        delay(100);
      }
      cacheFile.close();

      // Remove cache file after sending
      if (SPIFFS.exists("/cache.txt")) {
        File f = SPIFFS.open("/cache.txt", "r");
        if (f && !f.isDirectory()) {
          f.close();
          SPIFFS.remove("/cache.txt");
          Serial.println("Cache cleared.");
        }
      }
    }
  } else {
    // Save unsent payload to SPIFFS
    Serial.println("MQTT offline. Caching payload...");
    File cacheFile = SPIFFS.open("/cache.txt", FILE_APPEND);
    if (cacheFile) {
      cacheFile.println(payload);
      cacheFile.close();
      Serial.println("Payload cached.");
    } else {
      Serial.println("Failed to open cache file.");
    }
  }

  Serial.println("------------------------");
  delay(1000);
}
