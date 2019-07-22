// Licensed under the MIT license.

const char* SCOPE_ID = "<ENTER SCOPE ID HERE>";
const char* DEVICE_ID = "<ENTER DEVICE ID HERE>";
const char* DEVICE_KEY = "<ENTER DEVICE primary/secondary KEY HERE>";

#include <ESP8266WiFiMulti.h>

#include "lib/ArduinoTuya/ArduinoTuya.h"

#include "lib/iotc/common/string_buffer.h"
#include "lib/iotc/iotc.h"
void on_event(IOTContext ctx, IOTCallbackInfo* callbackInfo);
#include "lib/connection.h"

void on_event(IOTContext ctx, IOTCallbackInfo* callbackInfo) {
  // ConnectionStatus
  if (strcmp(callbackInfo->eventName, "ConnectionStatus") == 0) {
    LOG_VERBOSE("Is connected ? %s (%d)",
                callbackInfo->statusCode == IOTC_CONNECTION_OK ? "YES" : "NO",
                callbackInfo->statusCode);
    isConnected = callbackInfo->statusCode == IOTC_CONNECTION_OK;
    return;
  }

  // payload buffer doesn't have a null ending.
  // add null ending in another buffer before print
  AzureIOT::StringBuffer buffer;
  if (callbackInfo->payloadLength > 0) {
    buffer.initialize(callbackInfo->payload, callbackInfo->payloadLength);
  }

  LOG_VERBOSE("- [%s] event was received. Payload => %s\n",
              callbackInfo->eventName, buffer.getLength() ? *buffer : "EMPTY");

  if (strcmp(callbackInfo->eventName, "Command") == 0) {
    LOG_VERBOSE("- Command name was => %s\r\n", callbackInfo->tag);
  }
}

void setup() {

  // Set up serial  
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println();

  // Set up wifi
  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP("WIFI", "PASSWORD");
    
  Serial.print("Waiting for connection...");
  while (WiFiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println(" ready.");

  delay(1000);

  connect_client(SCOPE_ID, DEVICE_ID, DEVICE_KEY);

  if (context != NULL) {
    lastTick = 0;  // set timer in the past to enable first telemetry a.s.a.p
  }
}

void loop() {
  if (isConnected) {
    unsigned long ms = millis();
    if (ms - lastTick > 10000) {  // send telemetry every 10 seconds
      char msg[64] = {0};
      int pos = 0, errorCode = 0;

      lastTick = ms;
      if (loopId++ % 2 == 0) {  // send telemetry
        pos = snprintf(msg, sizeof(msg) - 1, "{\"accelerometerX\": %d}",
                       10 + (rand() % 20));
        errorCode = iotc_send_telemetry(context, msg, pos);
      } else {  // send property
        pos = snprintf(msg, sizeof(msg) - 1, "{\"dieNumber\":%d}",
                       1 + (rand() % 5));
        errorCode = iotc_send_property(context, msg, pos);
      }
      msg[pos] = 0;

      if (errorCode != 0) {
        LOG_ERROR("Sending message has failed with error code %d", errorCode);
      }
    }

    iotc_do_work(context);  // do background work for iotc
  } else {
    iotc_free_context(context);
    context = NULL;
    connect_client(SCOPE_ID, DEVICE_ID, DEVICE_KEY);
  }
}
