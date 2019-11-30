/*
  This shetch reads values from the Sensirion SCD30 CO2 sensor and transmits it via MQTT
 It can be adabted to the ESP32 by using the AsynchMQTT Library example of the ESP32 and by filling in the needed sensor commands from this sketch
  
  It is based on the examples of the libraries

  Copyright: Andreas Spiess 2019

  Copyright: Andreas Spiess 2019
*/
#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <AsyncMqttClient.h>
#include <Wire.h>
// #include <credentials.h>

unsigned long entryLoop, entryPublish;
static String payload;
int co2;
float temp, hum;

#define MQTT_HOST IPAddress(192, 168, 0, 203)
#define MQTT_PORT 1883

//Click here to get the library: http://librarymanager/All#SparkFun_SCD30
#include "SparkFun_SCD30_Arduino_Library.h"

SCD30 airSensor;
AsyncMqttClient mqttClient;
Ticker mqttReconnectTimer;

WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;
Ticker wifiReconnectTimer;

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
  mqttClient.publish("SDG30/status", 0, true, "SDG30 up");
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
  Serial.println("SCD30 Example");

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
  Wire.begin();   // ESP8266 SCL
  // Wire.setClock(100000);
  airSensor.begin(); //This will cause readings to occur every two seconds
  airSensor.setMeasurementInterval(2);
  //  airSensor.setAltitudeCompensation(350);
  airSensor.setAmbientPressure(1001);
  airSensor.beginMeasuring();
  while (!airSensor.dataAvailable()); {
    Serial.print(".");
    delay(100);
  }
  co2 = airSensor.getCO2();
  Serial.print("co2(ppm):");
  Serial.print(co2, 1);

  temp = airSensor.getTemperature();
  Serial.print(" temp(C):");
  Serial.print(temp);

  hum = airSensor.getHumidity();
  Serial.print(" humidity(%):");
  Serial.println(hum, 1);
  delay(100);

}

void loop()
{
  while (!airSensor.dataAvailable()) {
    Serial.print(".");
    delay(100);
  }
  co2 = ((19 * co2) + airSensor.getCO2()) / 20;
  Serial.print("co2(ppm):");
  Serial.print(co2, 1);

  temp = ((19 * temp) + airSensor.getTemperature()) / 20.0;
  Serial.print(" temp(C):");
  Serial.print(temp);

  hum = ((19 * hum) + airSensor.getHumidity()) / 20.0;
  Serial.print(" humidity(%):");
  Serial.print(hum, 1);

  Serial.print(" ");
  Serial.println((millis() - entryPublish) / 1000);

  if (millis() - entryPublish > 120000) {
    payload = "{\"co2 \":" + String(co2) + ", \"temperature \":" + temp + ", \"humidity \":" + hum + "}";
    Serial.println(payload);

    static char payloadStr[100];
    payload.toCharArray(payloadStr, payload.length() + 1);
    Serial.println(payloadStr);
    mqttClient.publish("SDG30/data", 0, true, payloadStr);
    entryPublish = millis();
  }
  Serial.println();
  while (millis() - entryLoop < 2500) yield();
  entryLoop = millis();
}
