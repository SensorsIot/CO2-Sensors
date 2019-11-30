/*
  This shetch reads values from the Sensirion SGP30 VOC sensor and transmits it via MQTT
 It can be adabted to the ESP32 by using the AsynchMQTT Library example of the ESP32 and by filling in the needed sensor commands from this sketch
  
  It is based on the examples of the libraries

  Copyright: Andreas Spiess 2019

  Copyright: Andreas Spiess 2019
*/

#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <AsyncMqttClient.h>
#include <Wire.h>
#include <Adafruit_SGP30.h>
#include <credentials.h>

unsigned int eco2;
unsigned int tvoc;
char payloadStr[100];
unsigned long entryLoop, entryPublish;

#define MQTT_HOST IPAddress(192, 168, 0, 203)
#define MQTT_PORT 1883

Adafruit_SGP30 sgp;

AsyncMqttClient mqttClient;
Ticker mqttReconnectTimer;

WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;
Ticker wifiReconnectTimer;

uint32_t getAbsoluteHumidity(float temperature, float humidity) {
  // approximation formula from Sensirion SGP30 Driver Integration chapter 3.15
  const float absoluteHumidity = 216.7f * ((humidity / 100.0f) * 6.112f * exp((17.62f * temperature) / (243.12f + temperature)) / (273.15f + temperature)); // [g/m^3]
  const uint32_t absoluteHumidityScaled = static_cast<uint32_t>(1000.0f * absoluteHumidity); // [mg/m^3]
  return absoluteHumidityScaled;
}

void connectToWifi() {
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(mySSID, myPASSWORD);
}

void onWifiConnect(const WiFiEventStationModeGotIP& event) {
  Serial.println("Connected to Wi-Fi.");
  connectToMqtt();
}

void onWifiDisconnect(const WiFiEventStationModeDisconnected& event) {
  Serial.println("Disconnected from Wi-Fi.");
  mqttReconnectTimer.detach(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
  wifiReconnectTimer.once(2, connectToWifi);
}

void connectToMqtt() {
  Serial.println("Connecting to MQTT...");
  mqttClient.connect();
}

void onMqttConnect(bool sessionPresent) {
  Serial.println("Connected to MQTT.");
  Serial.print("Session present: ");
  Serial.println(sessionPresent);
  uint16_t packetIdSub = mqttClient.subscribe("test/lol", 2);
  Serial.print("Subscribing at QoS 2, packetId: ");
  Serial.println(packetIdSub);
  mqttClient.publish("SGP30/status", 0, true, "SGP30 up");
  /*
    Serial.println("Publishing at QoS 0");
    uint16_t packetIdPub1 = mqttClient.publish("test/lol", 1, true, "test 2");
    Serial.print("Publishing at QoS 1, packetId: ");
    Serial.println(packetIdPub1);
    uint16_t packetIdPub2 = mqttClient.publish("test/lol", 2, true, "test 3");
    Serial.print("Publishing at QoS 2, packetId: ");
    Serial.println(packetIdPub2);
  */
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println("Disconnected from MQTT.");

  if (WiFi.isConnected()) {
    mqttReconnectTimer.once(2, connectToMqtt);
  }
}

void onMqttSubscribe(uint16_t packetId, uint8_t qos) {
  Serial.println("Subscribe acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
  Serial.print("  qos: ");
  Serial.println(qos);
}

void onMqttUnsubscribe(uint16_t packetId) {
  Serial.println("Unsubscribe acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}

void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
  Serial.println("Publish received.");
  Serial.print("  topic: ");
  Serial.println(topic);
  Serial.print("  qos: ");
  Serial.println(properties.qos);
  Serial.print("  dup: ");
  Serial.println(properties.dup);
  Serial.print("  retain: ");
  Serial.println(properties.retain);
  Serial.print("  len: ");
  Serial.println(len);
  Serial.print("  index: ");
  Serial.println(index);
  Serial.print("  total: ");
  Serial.println(total);
}

void onMqttPublish(uint16_t packetId) {
  Serial.println("Publish acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}


void setup()
{
  Serial.begin(115200);
  Serial.println("SGP30 test");

  if (! sgp.begin()) {
    Serial.println("Sensor not found :(");
    while (1);
  }
  Serial.print("Found SGP30 serial #");
  Serial.print(sgp.serialnumber[0], HEX);
  Serial.print(sgp.serialnumber[1], HEX);
  Serial.println(sgp.serialnumber[2], HEX);
  wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);
  wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onSubscribe(onMqttSubscribe);
  mqttClient.onUnsubscribe(onMqttUnsubscribe);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.onPublish(onMqttPublish);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);

  connectToWifi();
  Serial.println("WiFi connected");

  sgp.setHumidity(8200);  //8200 mg/m3 humidity (35% at 25 degrees

  // the first eco2 meaurements are always 400
  do {
    while (!sgp.IAQmeasure()) {
      Serial.print(".");
      delay(100);
    }
    tvoc = sgp.TVOC;
    Serial.print("TVOC(ppb):");
    Serial.print(tvoc);

    eco2 = sgp.eCO2;
    tvoc = sgp.TVOC;
    Serial.print(" eco2(ppm):");
    Serial.println(eco2);
    delay(1000);
  } while (eco2 == 400);
  entryLoop = millis();
}

void loop()
{
  static String payload;
  while (!sgp.IAQmeasure()) {
    Serial.print(".");
    delay(100);
  }

  tvoc = ((19 * tvoc) + sgp.TVOC) / 20;
  Serial.print("TVOC(ppb):");
  Serial.print(tvoc);

  eco2 = ((19 * eco2) + sgp.eCO2) / 20;
  Serial.print(" eco2(ppm):");
  Serial.print(eco2);


  while (!sgp.IAQmeasureRaw()) {
    Serial.print(".");
    delay(100);
  }
  unsigned int rawh2 = sgp.rawH2;
  Serial.print(" rawH2:");
  Serial.print(rawh2);

  unsigned int ethanol = sgp.rawEthanol;
  Serial.print(" Ethanol:");
  Serial.print(ethanol);

  Serial.print(" ");
  Serial.println((millis() - entryPublish) / 1000);

  if (millis() - entryPublish > 120000) {
    payload = "{\"tvoc\":" + String(tvoc) + ", \"eco2\":" + String(eco2) + ", \"rawh2\":" + String(rawh2) + ", \"ethanol\":" + String(ethanol) + "}";
    Serial.println(payload);
    payload.toCharArray(payloadStr, payload.length() + 1);
    mqttClient.publish("SGP30/data", 0, true, payloadStr);
    entryPublish = millis();
  } 
  Serial.println();
  while (millis() - entryLoop < 1000) yield();
  entryLoop = millis();

}
