// Licensed under the MIT license.

const char* SCOPE_ID = "";
const char* DEVICE_ID = "";
const char* DEVICE_KEY = "";

// WiFi
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
ESP8266WiFiMulti wifiMulti;

// IoTC
#include "src/iotc/common/string_buffer.h"
#include "src/iotc/iotc.h"
void on_event(IOTContext ctx, IOTCallbackInfo* callbackInfo);
#include "src/connection.h"

// JSON
#include <ArduinoJson.h>

// Smartplugs
#include "src/ArduinoTuya/ArduinoTuya.h"

// IR
#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
IRsend irsend(D3);

bool state, state_vert, state_horiz;

void setup() {
  
  // Serial
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println();

  // GPIO
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  // IR
  irsend.begin();

  // WiFi
  WiFi.mode(WIFI_STA);
  wifiMulti.addAP("WIFI", "PASSWORD");
  while (wifiMulti.run() != WL_CONNECTED) {
    delay(100);
  }
  Serial.println();
  Serial.print("MAC: ");
  Serial.println(WiFi.macAddress());
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("ID: ");
  Serial.println(DEVICE_ID);
  Serial.println();

  // State
  lastTick = 0;
  isConnected = false;

  // Start
  connect_client(SCOPE_ID, DEVICE_ID, DEVICE_KEY);
  iotc_do_work(context);
  Serial.println("Ready");

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
  }  
}

void on_event(IOTContext ctx, IOTCallbackInfo* callbackInfo) {

  if (strcmp(callbackInfo->eventName, "ConnectionStatus") == 0) {
    
    LOG_VERBOSE("Is connected ? %s (%d)",
                callbackInfo->statusCode == IOTC_CONNECTION_OK ? "YES" : "NO",
                callbackInfo->statusCode);
                
    isConnected = callbackInfo->statusCode == IOTC_CONNECTION_OK;

    sendState();
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

    if (strcmp(callbackInfo->tag, "SendIR") == 0) {
      DynamicJsonDocument msg(JSON_OBJECT_SIZE(10));
      auto error = deserializeJson(msg, callbackInfo->payload, callbackInfo->payloadLength);
      if (error) {
        Serial.println("Could not deserialize");
      } else {
        irsend.sendNEC(msg["data"]);
      }
    }
    if (strcmp(callbackInfo->tag, "FAN_POWER") == 0) {
      irsend.sendNEC(0xCF331CE);
      state = !state;
      sendState();
    }
    if (strcmp(callbackInfo->tag, "FAN_ON") == 0) {
      if (!state) {
        irsend.sendNEC(0xCF331CE);
        state = true;
        sendState();
      }
    }
    if (strcmp(callbackInfo->tag, "FAN_OFF") == 0) {
      if (state) {
        irsend.sendNEC(0xCF331CE);
        state = false;
        sendState();
      }
    }
    if (strcmp(callbackInfo->tag, "FAN_VERT") == 0) {
      irsend.sendNEC(0xCF3A956);
      state_vert = !state_vert;
      sendState();
    }
    if (strcmp(callbackInfo->tag, "FAN_VERT_ON") == 0) {
      if (!state_vert) {
        irsend.sendNEC(0xCF3A956);
        state_vert = true;
        sendState();
      }
    }
    if (strcmp(callbackInfo->tag, "FAN_VERT_OFF") == 0) {
      if (state_vert) {
        irsend.sendNEC(0xCF3A956);
        state_vert = false;
        sendState();
      }
    }
    if (strcmp(callbackInfo->tag, "FAN_HORIZ") == 0) {
      irsend.sendNEC(0xCF339C6);
      state_horiz = !state_horiz;
      sendState();
    }
    if (strcmp(callbackInfo->tag, "FAN_HORIZ_ON") == 0) {
      if (!state_horiz) {
        irsend.sendNEC(0xCF339C6);
        state_horiz = true;
        sendState();
      }
    }
    if (strcmp(callbackInfo->tag, "FAN_HORIZ_OFF") == 0) {
      if (state_horiz) {
        irsend.sendNEC(0xCF339C6);
        state_horiz = false;
        sendState();
      }
    }
    if (strcmp(callbackInfo->tag, "FAN_TURBO") == 0) {
      irsend.sendNEC(0xCF311EE);
    }
  }

}

void sendState() {
  DynamicJsonDocument doc(JSON_OBJECT_SIZE(3));
  if (state_horiz && state_vert) {
    doc["oscillation"] = "BOTH";
  } else if (state_horiz) {
    doc["oscillation"] = "HORIZONTAL";
  } else if (state_vert) {
    doc["oscillation"] = "VERTICAL";
  } else {
    doc["oscillation"] = "NONE";
  }
  doc["ventilation"] = state ? "ON" : "OFF";
  
  char msg[128];
  int errorCode = iotc_send_state(context, msg, serializeJson(doc, msg));
  Serial.println(msg);
  delay(500);
}
