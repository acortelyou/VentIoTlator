// Licensed under the MIT license.

// WiFi
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
ESP8266WiFiMulti wifiMulti;

// Sensors
#include <Wire.h>
#include "ClosedCube_SHT31D.h"
ClosedCube_SHT31D sht3xd;

// IoTC
#include "src/iotc/common/string_buffer.h"
#include "src/iotc/iotc.h"
void on_event(IOTContext ctx, IOTCallbackInfo* callbackInfo);
#include "src/connection.h"

// JSON
#include <ArduinoJson.h>

// Smartplugs
#include "src/ArduinoTuya/ArduinoTuya.h"

void setup() {
  
  // Serial
  Serial.begin(115200);
  Serial.println();

  // GPIO
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  // Sensors
  Wire.begin();
  sht3xd.begin(0x45);

  // WiFi
  WiFi.mode(WIFI_STA);
  wifiMulti.addAP("WIFI", "PASSWORD");
  while (wifiMulti.run() != WL_CONNECTED) {
    delay(100);
  }

  // State
  lastTick = 0;
  isConnected = false;
  
  
}

void loop() {

  // Wait for WiFi
  while (wifiMulti.run() != WL_CONNECTED) {
    delay(100);
    isConnected = false;
  }

  // Connect to IoTC
  if (!isConnected) {
    iotc_free_context(context);
    context = NULL;
    connect_client(SCOPE_ID, DEVICE_ID, DEVICE_KEY);
  } else {
    iotc_do_work(context);

    // Every ten seconds
    unsigned long ms = millis();
    if (ms - lastTick > 10000) {  
      lastTick = ms;

      // Read sensor
      SHT31D result = sht3xd.readTempAndHumidity(SHT3XD_REPEATABILITY_HIGH, SHT3XD_MODE_POLLING, 50);

      // Build message
      DynamicJsonDocument doc(JSON_OBJECT_SIZE(3));
      if (result.error == SHT3XD_NO_ERROR) {
        doc["temperature"] = result.t;
        doc["humidity"] = result.rh;
      }
      doc["error"] = (int)result.error;
      char msg[128];
      int len = serializeJson(doc, msg);

      // Send message
      int errorCode = iotc_send_telemetry(context, msg, len);
      if (errorCode != 0) {
        LOG_ERROR("Sending message has failed with error code %d", errorCode);
      }
    }
  }  
}

void on_event(IOTContext ctx, IOTCallbackInfo* callbackInfo) {

  if (strcmp(callbackInfo->eventName, "ConnectionStatus") == 0) {
    
    LOG_VERBOSE("Is connected ? %s (%d)",
                callbackInfo->statusCode == IOTC_CONNECTION_OK ? "YES" : "NO",
                callbackInfo->statusCode);
                
    isConnected = callbackInfo->statusCode == IOTC_CONNECTION_OK;
    return;
  }
  
  AzureIOT::StringBuffer buffer;
  if (callbackInfo->payloadLength > 0) {
    buffer.initialize(callbackInfo->payload, callbackInfo->payloadLength);
  }

  LOG_VERBOSE("- [%s] event was received. Payload => %s\n",
              callbackInfo->eventName,
              buffer.getLength() ? *buffer : "EMPTY");

  if (strcmp(callbackInfo->eventName, "Command") == 0) {
    LOG_VERBOSE("- Command name was => %s\n", callbackInfo->tag);
  }
}
