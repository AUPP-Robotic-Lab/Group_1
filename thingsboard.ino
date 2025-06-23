#if defined(ESP8266)
#include <ESP8266WiFi.h>
#define THINGSBOARD_ENABLE_PROGMEM 0
#elif defined(ESP32) || defined(RASPBERRYPI_PICO) || defined(RASPBERRYPI_PICO_W)
#include <WiFi.h>
#endif

#ifndef LED_BUILTIN
#define LED_BUILTIN 99
#endif

#include <Arduino_MQTT_Client.h>
#include <Server_Side_RPC.h>
#include <Attribute_Request.h>
#include <Shared_Attribute_Update.h>
#include <ThingsBoard.h>
#include <Adafruit_BMP280.h>
#include <Wire.h>

// BMP280 Setup
Adafruit_BMP280 bmp; // use I2C

// WiFi credentials
constexpr char WIFI_SSID[] = "Robotic WIFI";
constexpr char WIFI_PASSWORD[] = "rbtWIFI@2025";

// ThingsBoard token and server
constexpr char TOKEN[] = "lgxpw7ves00jcnmls8ae";
constexpr char THINGSBOARD_SERVER[] = "thingsboard.cloud";
constexpr uint16_t THINGSBOARD_PORT = 1883U;

constexpr uint32_t MAX_MESSAGE_SIZE = 1024U;
constexpr uint32_t SERIAL_DEBUG_BAUD = 115200U;
constexpr size_t MAX_ATTRIBUTES = 3U;
constexpr uint64_t REQUEST_TIMEOUT_MICROSECONDS = 5000U * 1000U;

constexpr const char BLINKING_INTERVAL_ATTR[] = "blinkingInterval";
constexpr const char LED_MODE_ATTR[] = "ledMode";
constexpr const char LED_STATE_ATTR[] = "ledState";

// MQTT and ThingsBoard clients
WiFiClient wifiClient;
Arduino_MQTT_Client mqttClient(wifiClient);
Server_Side_RPC<3U, 5U> rpc;
Attribute_Request<2U, MAX_ATTRIBUTES> attr_request;
Shared_Attribute_Update<3U, MAX_ATTRIBUTES> shared_update;

const std::array<IAPI_Implementation*, 3U> apis = {
  &rpc,
  &attr_request,
  &shared_update
};

ThingsBoard tb(mqttClient, MAX_MESSAGE_SIZE, Default_Max_Stack_Size, apis);

// LED and control state
volatile bool attributesChanged = false;
volatile int ledMode = 0;
volatile bool ledState = false;
constexpr uint16_t BLINKING_INTERVAL_MS_MIN = 10U;
constexpr uint16_t BLINKING_INTERVAL_MS_MAX = 60000U;
volatile uint16_t blinkingInterval = 1000U;
uint32_t previousStateChange;

constexpr int16_t telemetrySendInterval = 2000U;
uint32_t previousDataSend;

constexpr std::array<const char *, 2U> SHARED_ATTRIBUTES_LIST = {
  LED_STATE_ATTR,
  BLINKING_INTERVAL_ATTR
};

constexpr std::array<const char *, 1U> CLIENT_ATTRIBUTES_LIST = {
  LED_MODE_ATTR
};

void InitWiFi() {
  Serial.println("Connecting to AP ...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected to AP");
}

const bool reconnect() {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }
  InitWiFi();
  return true;
}

void processSetLedMode(const JsonVariantConst &data, JsonDocument &response) {
  Serial.println("Received the set led state RPC method");

  int new_mode = data;
  Serial.print("Mode to change: ");
  Serial.println(new_mode);

  StaticJsonDocument<1> response_doc;

  if (new_mode != 0 && new_mode != 1) {
    response_doc["error"] = "Unknown mode!";
    response.set(response_doc);
    return;
  }

  ledMode = new_mode;
  attributesChanged = true;
  response_doc["newMode"] = (int)ledMode;
  response.set(response_doc);
}

const std::array<RPC_Callback, 1U> callbacks = {
  RPC_Callback{ "setLedMode", processSetLedMode }
};

void processSharedAttributes(const JsonObjectConst &data) {
  for (auto it = data.begin(); it != data.end(); ++it) {
    if (strcmp(it->key().c_str(), BLINKING_INTERVAL_ATTR) == 0) {
      const uint16_t new_interval = it->value().as<uint16_t>();
      if (new_interval >= BLINKING_INTERVAL_MS_MIN && new_interval <= BLINKING_INTERVAL_MS_MAX) {
        blinkingInterval = new_interval;
        Serial.print("Blinking interval is set to: ");
        Serial.println(new_interval);
      }
    } else if (strcmp(it->key().c_str(), LED_STATE_ATTR) == 0) {
      ledState = it->value().as<bool>();
      if (LED_BUILTIN != 99) {
        digitalWrite(LED_BUILTIN, ledState);
      }
      Serial.print("LED state is set to: ");
      Serial.println(ledState);
    }
  }
  attributesChanged = true;
}

void processClientAttributes(const JsonObjectConst &data) {
  for (auto it = data.begin(); it != data.end(); ++it) {
    if (strcmp(it->key().c_str(), LED_MODE_ATTR) == 0) {
      const uint16_t new_mode = it->value().as<uint16_t>();
      ledMode = new_mode;
    }
  }
}

void requestTimedOut() {
  Serial.printf("Attribute request timed out after (%llu) microseconds.\n", REQUEST_TIMEOUT_MICROSECONDS);
}

const Shared_Attribute_Callback<MAX_ATTRIBUTES> attributes_callback(&processSharedAttributes, SHARED_ATTRIBUTES_LIST.cbegin(), SHARED_ATTRIBUTES_LIST.cend());
const Attribute_Request_Callback<MAX_ATTRIBUTES> attribute_shared_request_callback(&processSharedAttributes, REQUEST_TIMEOUT_MICROSECONDS, &requestTimedOut, SHARED_ATTRIBUTES_LIST);
const Attribute_Request_Callback<MAX_ATTRIBUTES> attribute_client_request_callback(&processClientAttributes, REQUEST_TIMEOUT_MICROSECONDS, &requestTimedOut, CLIENT_ATTRIBUTES_LIST);

void setup() {
  Serial.begin(SERIAL_DEBUG_BAUD);
  if (LED_BUILTIN != 99) {
    pinMode(LED_BUILTIN, OUTPUT);
  }
  delay(1000);

  InitWiFi();

  // Initialize BMP280
  if (!bmp.begin(0x77)) { // Try 0x77 if this fails
    Serial.println("Could not find BMP280 sensor!");
    while (1);
  }
}

void loop() {
  delay(10);

  if (!reconnect()) return;

  if (!tb.connected()) {
    Serial.print("Connecting to: ");
    Serial.print(THINGSBOARD_SERVER);
    Serial.print(" with token ");
    Serial.println(TOKEN);
    if (!tb.connect(THINGSBOARD_SERVER, TOKEN, THINGSBOARD_PORT)) {
      Serial.println("Failed to connect");
      return;
    }

    tb.sendAttributeData("macAddress", WiFi.macAddress().c_str());

    if (!rpc.RPC_Subscribe(callbacks.cbegin(), callbacks.cend())) {
      Serial.println("Failed to subscribe for RPC");
      return;
    }

    if (!shared_update.Shared_Attributes_Subscribe(attributes_callback)) {
      Serial.println("Failed to subscribe for shared attribute updates");
      return;
    }

    if (!attr_request.Shared_Attributes_Request(attribute_shared_request_callback)) {
      Serial.println("Failed to request for shared attributes");
      return;
    }

    if (!attr_request.Client_Attributes_Request(attribute_client_request_callback)) {
      Serial.println("Failed to request for client attributes");
      return;
    }
  }

  if (attributesChanged) {
    attributesChanged = false;
    if (ledMode == 0) {
      previousStateChange = millis();
    }
    tb.sendTelemetryData(LED_MODE_ATTR, ledMode);
    tb.sendTelemetryData(LED_STATE_ATTR, ledState);
    tb.sendAttributeData(LED_MODE_ATTR, ledMode);
    tb.sendAttributeData(LED_STATE_ATTR, ledState);
  }

  if (ledMode == 1 && millis() - previousStateChange > blinkingInterval) {
    previousStateChange = millis();
    ledState = !ledState;
    tb.sendTelemetryData(LED_STATE_ATTR, ledState);
    tb.sendAttributeData(LED_STATE_ATTR, ledState);
    if (LED_BUILTIN != 99) {
      digitalWrite(LED_BUILTIN, ledState);
    } else {
      Serial.print("LED state changed to: ");
      Serial.println(ledState);
    }
  }

  // Send sensor data every interval
  if (millis() - previousDataSend > telemetrySendInterval) {
    previousDataSend = millis();

    float temp = bmp.readTemperature();
    float pressure = bmp.readPressure();
    float altitude = bmp.readAltitude(1013.25); // You can adjust sea-level pressure

    Serial.print("Real Temperature: "); Serial.println(temp);
    Serial.print("Real Pressure: "); Serial.println(pressure);
    Serial.print("Real Altitude: "); Serial.println(altitude);

    tb.sendTelemetryData("temperature", temp);
    tb.sendTelemetryData("pressure", pressure);
    tb.sendTelemetryData("altitude", altitude);

    tb.sendAttributeData("rssi", WiFi.RSSI());
    tb.sendAttributeData("channel", WiFi.channel());
    tb.sendAttributeData("bssid", WiFi.BSSIDstr().c_str());
    tb.sendAttributeData("localIp", WiFi.localIP().toString().c_str());
    tb.sendAttributeData("ssid", WiFi.SSID().c_str());
  }

  tb.loop();
}
