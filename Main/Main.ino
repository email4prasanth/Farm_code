#include "secrets.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_SHT31.h>

// ======================================================
// SHT31 configuration
// ======================================================

#define SDA_PIN 21
#define SCL_PIN 22

#define SHT31_ADDRESS 0x44

// ======================================================
// AWS IoT MQTT configuration
// ======================================================

#define AWS_IOT_PUBLISH_TOPIC "mushroom/room-01/telemetry"

// ======================================================
// Objects
// ======================================================

Adafruit_SHT31 sht31 = Adafruit_SHT31();

WiFiClientSecure net;
PubSubClient client(net);

// ======================================================
// Connect to Wi-Fi
// ======================================================

void connectWiFi()
{
  Serial.println();
  Serial.println("Connecting to Wi-Fi...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("Wi-Fi connected");

  Serial.print("ESP32 IP address: ");
  Serial.println(WiFi.localIP());
}

// ======================================================
// Connect to AWS IoT Core
// ======================================================

void connectAWS()
{
  Serial.println();
  Serial.println("Connecting to AWS IoT Core...");

  while (!client.connected())
  {
    Serial.print("MQTT connection... ");

    if (client.connect(THINGNAME))
    {
      Serial.println("SUCCESS");
    }
    else
    {
      Serial.print("FAILED, state=");
      Serial.println(client.state());

      delay(5000);
    }
  }

  Serial.println("AWS IoT Core connected!");
}

// ======================================================
// Publish SHT31 data
// ======================================================

void publishSensorData()
{
  float temperature = sht31.readTemperature();
  float humidity = sht31.readHumidity();

  // Check sensor readings
  if (isnan(temperature) || isnan(humidity))
  {
    Serial.println("ERROR: Failed to read SHT31");
    return;
  }

  // Display locally
  Serial.println("--------------------------------");
  Serial.print("Temperature: ");
  Serial.print(temperature, 2);
  Serial.println(" °C");

  Serial.print("Humidity: ");
  Serial.print(humidity, 2);
  Serial.println(" %");

  // Create MQTT JSON payload
  char payload[200];

  snprintf(
    payload,
    sizeof(payload),
    "{\"device_id\":\"%s\",\"temperature_c\":%.2f,\"humidity_pct\":%.2f}",
    THINGNAME,
    temperature,
    humidity
  );

  Serial.print("MQTT Payload: ");
  Serial.println(payload);

  // Publish
  if (client.publish(AWS_IOT_PUBLISH_TOPIC, payload))
  {
    Serial.println("AWS IoT publish: SUCCESS");
  }
  else
  {
    Serial.println("AWS IoT publish: FAILED");
  }
}

// ======================================================
// Setup
// ======================================================

void setup()
{
  Serial.begin(115200);

  delay(1000);

  Serial.println();
  Serial.println("======================================");
  Serial.println("ESP32 + SHT31 + AWS IoT Core");
  Serial.println("======================================");

  // ----------------------------------------------------
  // Initialize I2C
  // ----------------------------------------------------

  Wire.begin(SDA_PIN, SCL_PIN);

  Serial.println("Initializing SHT31...");

  if (!sht31.begin(SHT31_ADDRESS))
  {
    Serial.println("ERROR: SHT31 not detected!");
    Serial.println("Check wiring and I2C address.");
    
    while (true)
    {
      delay(1000);
    }
  }

  Serial.println("SHT31 detected successfully.");
  Serial.println("I2C address: 0x44");

  // ----------------------------------------------------
  // Connect Wi-Fi
  // ----------------------------------------------------

  connectWiFi();

  // ----------------------------------------------------
  // Configure TLS
  // ----------------------------------------------------

  Serial.println("Configuring AWS TLS certificates...");

  net.setCACert(AWS_CERT_CA);
  net.setCertificate(AWS_CERT_CRT);
  net.setPrivateKey(AWS_CERT_PRIVATE);

  // ----------------------------------------------------
  // Configure MQTT
  // ----------------------------------------------------

  client.setServer(AWS_IOT_ENDPOINT, 8883);

  // ----------------------------------------------------
  // Connect AWS IoT Core
  // ----------------------------------------------------

  connectAWS();

  Serial.println();
  Serial.println("System ready.");
  Serial.println("Publishing sensor data every 1 second.");
}

// ======================================================
// Main loop
// ======================================================

void loop()
{
  // Keep MQTT connection alive
  client.loop();

  // ----------------------------------------------------
  // Check Wi-Fi
  // ----------------------------------------------------

  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("Wi-Fi disconnected.");

    connectWiFi();
  }

  // ----------------------------------------------------
  // Check AWS connection
  // ----------------------------------------------------

  if (!client.connected())
  {
    Serial.println("AWS IoT disconnected.");

    connectAWS();
  }

  // ----------------------------------------------------
  // Read and publish every 1 second
  // ----------------------------------------------------

  publishSensorData();

  delay(1000);
}