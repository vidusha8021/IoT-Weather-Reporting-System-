
#include <WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"
#include "Wire.h"
#include "LiquidCrystal_I2C.h"
#include <math.h>

// Pin Definitions
#define POWER_PIN 32 // ESP32's pin GPIO32 that provides the power to the rain sensor
#define DO_PIN 13 // ESP32's pin GPIO13 connected to DO pin of the rain sensor
#define DHT_SENSOR_PIN 21 // ESP32 pin GPIO21 connected to DHT22 sensor
#define BUZZER_PIN 25 // ESP32 pin GPIO25 connected to Buzzer
#define LIGHT_SENSOR_PIN 36 // ESP32 pin GPIO36 (ADC0)

// WiFi credentials
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// ThingsBoard connection details
const char* mqttServer = "cloud.thingsboard.io";
const int mqttPort = 1883;
const char* deviceAccessToken = "efGuQHmJjG3czPmQrBiO";

// Create a WiFiClient and PubSubClient objects
WiFiClient espClient;
PubSubClient client(espClient);

// LCD object
LiquidCrystal_I2C lcd(0x27, 20, 2);

// DHT Sensor Object
DHT dht_sensor(DHT_SENSOR_PIN, DHT22);

// Light sensor variables
float GAMMA = 0.7;
float RL10 = 85;

void setup() {
  // Initialize Serial Communication
  Serial.begin(9600);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(3000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  // Set up MQTT connection
  client.setServer(mqttServer, mqttPort);

  // Initialize the Arduino's pins as input/output pins
  pinMode(POWER_PIN, OUTPUT);
  pinMode(DO_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  // Initialize the DHT sensor
  dht_sensor.begin();

  // Initialize LCD
  lcd.begin(20, 2);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Weather Report");
  lcd.setCursor(0, 1);
  lcd.print("Group-6");
  delay(100); // Wait for 3 seconds to show initial message
}

void loop() {
  // Ensure MQTT connection
  if (!client.connected()) {
    reconnect();
  }

  // Read temperature and humidity from DHT sensor
  float humi = dht_sensor.readHumidity();
  float tempC = dht_sensor.readTemperature();
  float tempF = tempC * 9.0 / 5.0 + 32;

  // Display temperature and humidity on LCD
  lcd.clear(); // Clear the LCD before writing
  lcd.setCursor(0, 0);
  lcd.print("Temp: ");
  lcd.print(tempC);
  lcd.print("C");

  lcd.setCursor(0, 1);
  lcd.print("Humidity: ");
  lcd.print(humi);
  lcd.print("%");

  // Serial print for debugging
  Serial.print("Temperature (C): ");
  Serial.println(tempC);
  Serial.print("Humidity: ");
  Serial.println(humi);

  // Publish to ThingsBoard
  publishTelemetry("Temperature", String(tempC));
  publishTelemetry("Humidity", String(humi));

  delay(3000); // Delay to display temperature and humidity

  // Rain Sensor Section
  digitalWrite(POWER_PIN, HIGH); // Power the rain sensor
  delay(10); // Small delay to stabilize sensor
  int rain_state = digitalRead(DO_PIN); // Read rain sensor state
  digitalWrite(POWER_PIN, LOW); // Turn off rain sensor

  // Display weather status on LCD
  lcd.clear(); // Clear the LCD before writing
  lcd.setCursor(0, 0);
  if (rain_state == LOW) {
    lcd.print("Weather: Clear");
    digitalWrite(BUZZER_PIN, LOW); // Turn off buzzer
  } else {
    lcd.print("Weather: Rainy");
    digitalWrite(BUZZER_PIN, HIGH); // Turn on buzzer
  }

  // Serial print for debugging
  Serial.print("Weather: ");
  Serial.println(rain_state == LOW ? "Clear" : "Rainy");

  // Publish weather status to ThingsBoard
  publishTelemetry("Weather", rain_state == LOW ? "Clear" : "Rainy");

  delay(3000); // Delay to display weather status

  // Light Sensor Section
  int analog_value = analogRead(LIGHT_SENSOR_PIN);
  float voltage = analog_value / 4095.0 * 3.3;
  float resistance = 5000 * voltage / (1 - voltage / 3.3);
  float lux = pow(RL10 * 1000 * pow(10, GAMMA) / resistance, (1 / GAMMA));

  // Determine light intensity category
  String lightCategory;
  if (lux > 0.1 && lux <= 1) {
    lightCategory = "Dark";
  } else if (lux > 1 && lux <= 10) {
    lightCategory = "Dim";
  } else if (lux > 10 && lux <= 100) {
    lightCategory = "Dull";
  } else if (lux > 100 && lux <= 1000) {
    lightCategory = "Drab";
  } else if (lux > 1000 && lux <= 5000) {
    lightCategory = "Gloomy";
  } else if (lux > 5000 && lux <= 10000) {
    lightCategory = "Glowing";
  } else if (lux > 10000 && lux <= 25000) {
    lightCategory = "Shining";
  } else if (lux > 25000 && lux <= 50000) {
    lightCategory = "Bright";
  } else if (lux > 50000) {
    lightCategory = "Luminous";
  } else {
    lightCategory = "Out of range";
  }

  // Display lux and light category on LCD
  lcd.clear(); // Clear the LCD before writing
  lcd.setCursor(0, 0);
  lcd.print("Lux: ");
  lcd.print(lux);

  lcd.setCursor(0, 1);
  lcd.print("Light: ");
  lcd.print(lightCategory);

  // Serial print for debugging
  Serial.print("Lux: ");
  Serial.println(lux);
  Serial.print("Brightness: ");
  Serial.println(lightCategory);

  // Publish lux data to ThingsBoard
  publishTelemetry("Brightness", lightCategory);
  publishTelemetry("Illumination", String(lux));

  delay(3000); // Delay to display lux and light intensity
}

// Reconnect function to ThingsBoard
void reconnect() {
  while (!client.connected()) {
    Serial.print("Connecting to ThingsBoard...");
    if (client.connect("ESP32 Device", deviceAccessToken, NULL)) {
      Serial.println("Connected");
    } else {
      Serial.print("Failed, rc=");
      Serial.print(client.state());
      Serial.println("Retrying in 3 seconds...");
      delay(3000);
    }
  }
}

// Publish telemetry to ThingsBoard
void publishTelemetry(const char* key, String value) {
  String telemetryTopic = String("v1/devices/me/telemetry");
  String payload = String("{\"") + key + String("\":\"") + value + String("\"}");
  client.publish(telemetryTopic.c_str(), payload.c_str());
}
