#include <PubSubClient.h>
#include <EEPROM.h>
#include "restart.h"

#define MQTT_attr "espaltherma/ATTR"
#define MQTT_lwt "espaltherma/LWT"

#define EEPROM_CHK 0
#define EEPROM_CHK_VAL 'X'

// #define LOG_DEBUG 1 

#ifdef PIN_THERM_MAIN
#define EEPROM_STATE_MAIN 1
#endif

#ifdef PIN_THERM_ADD
#define EEPROM_STATE_ADD 2
#endif

#ifdef PIN_THERM
#define EEPROM_STATE 3
#endif

#define MQTT_attr "espaltherma/ATTR"
#define MQTT_lwt "espaltherma/LWT"

#ifdef JSONTABLE
char jsonbuff[MAX_MSG_SIZE] = "[{\0";
#else
char jsonbuff[MAX_MSG_SIZE] = "{\0";
#endif

WiFiClient espClient;
PubSubClient client(espClient);

void sendValues()
{
  Serial.printf("Sending values in MQTT.\n");
#ifdef ARDUINO_M5Stick_C
  //Add M5 APX values
  snprintf(jsonbuff + strlen(jsonbuff),MAX_MSG_SIZE - strlen(jsonbuff) , "\"%s\":\"%.3gV\",\"%s\":\"%gmA\",", "M5VIN", M5.Axp.GetVinVoltage(),"M5AmpIn", M5.Axp.GetVinCurrent());
  snprintf(jsonbuff + strlen(jsonbuff),MAX_MSG_SIZE - strlen(jsonbuff) , "\"%s\":\"%.3gV\",\"%s\":\"%gmA\",", "M5BatV", M5.Axp.GetBatVoltage(),"M5BatCur", M5.Axp.GetBatCurrent());
  snprintf(jsonbuff + strlen(jsonbuff),MAX_MSG_SIZE - strlen(jsonbuff) , "\"%s\":\"%.3gmW\",", "M5BatPwr", M5.Axp.GetBatPower());
#endif
  snprintf(jsonbuff + strlen(jsonbuff),MAX_MSG_SIZE - strlen(jsonbuff) , "\"%s\":\"%ddBm\",", "WifiRSSI", WiFi.RSSI());
  snprintf(jsonbuff + strlen(jsonbuff),MAX_MSG_SIZE - strlen(jsonbuff) , "\"%s\":\"%d\",", "FreeMem", ESP.getFreeHeap());
  jsonbuff[strlen(jsonbuff) - 1] = '}';
#ifdef JSONTABLE
  strcat(jsonbuff,"]");
#endif
  client.publish(MQTT_attr, jsonbuff);
#ifdef JSONTABLE
  strcpy(jsonbuff, "[{\0");
#else
  strcpy(jsonbuff, "{\0");
#endif
}

void readEEPROM(){
  if (EEPROM_CHK_VAL == EEPROM.read(EEPROM_CHK)){
  
  #ifdef PIN_THERM
    digitalWrite(PIN_THERM,EEPROM.read(EEPROM_STATE));
    mqttSerial.printf("Restoring previous THERM state: %s",(EEPROM.read(EEPROM_STATE) == HIGH)? "Off":"On" );
  #endif

  #ifdef PIN_THERM_MAIN
    digitalWrite(PIN_THERM_MAIN,EEPROM.read(EEPROM_STATE_MAIN));
    mqttSerial.printf("Restoring previous THERM_MAIN state: %s",(EEPROM.read(EEPROM_STATE_MAIN) == HIGH)? "On":"Off" );
  #endif

  #ifdef PIN_THERM_ADD
    digitalWrite(PIN_THERM_ADD,EEPROM.read(EEPROM_STATE_ADD));
    mqttSerial.printf("Restoring previous THERM_ADD state: %s",(EEPROM.read(EEPROM_STATE_ADD) == HIGH)? "On":"Off" );
  #endif

  }
  else{
    mqttSerial.printf("EEPROM not initialized (%d). Initializing...",EEPROM.read(EEPROM_CHK));
    EEPROM.write(EEPROM_CHK,EEPROM_CHK_VAL);
  #ifdef PIN_THERM
    EEPROM.write(EEPROM_STATE,HIGH);
    digitalWrite(PIN_THERM,HIGH);
  #endif
  #ifdef PIN_THERM_MAIN
    EEPROM.write(EEPROM_STATE_MAIN,HIGH);
    digitalWrite(PIN_THERM_MAIN,HIGH);
  #endif
  #ifdef PIN_THERM_ADD
    EEPROM.write(EEPROM_STATE_ADD,HIGH);
    digitalWrite(PIN_THERM_ADD,HIGH);
  #endif
    EEPROM.commit();
  }
}

#define AVAILABILITY "\"availability\":[{\"topic\":\"espaltherma/LWT\",\"payload_available\":\"Online\",\"payload_not_available\":\"Offline\"}],\"availability_mode\":\"all\"" 
#define DEVICE_ID "\"device\":{\"identifiers\":[\"ESPAltherma\"]}"
void reconnectMqtt()
{
  // Loop until we're reconnected
  int i = 0;
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");

    if (client.connect("ESPAltherma-dev", MQTT_USERNAME, MQTT_PASSWORD, MQTT_lwt, 0, true, "Offline"))
    {
      Serial.println("connected!");
      client.publish("homeassistant/sensor/espAltherma/config", "{\"name\":\"AlthermaSensors\",\"stat_t\":\"~/LWT\",\"avty_t\":\"~/LWT\",\"pl_avail\":\"Online\",\"pl_not_avail\":\"Offline\",\"uniq_id\":\"espaltherma\",\"device\":{\"identifiers\":[\"ESPAltherma\"]}, \"~\":\"espaltherma\",\"json_attr_t\":\"~/ATTR\"}", true);
      client.publish(MQTT_lwt, "Online", true);

#ifdef PIN_THERM
      client.publish("homeassistant/switch/espAltherma/config", "{\"name\":\"Altherma\",\"cmd_t\":\"~/POWER\",\"stat_t\":\"~/STATE\",\"pl_off\":\"OFF\",\"pl_on\":\"ON\",\"~\":\"espaltherma\"}", true);

      // Subscribe
      client.subscribe("espaltherma/POWER");
#endif

#ifdef PIN_THERM_MAIN
      mqttSerial.print("Configuring HA switch for thermMain");
      client.publish("homeassistant/switch/espAltherma/thermMain/config", "{" AVAILABILITY "," DEVICE_ID ",\"unique_id\":\"AlthermaMainZone\",\"name\":\"AlthermaMainZone\",\"cmd_t\":\"~/thermMain/set\",\"stat_t\":\"~/thermMain/state\",\"pl_off\":\"OFF\",\"pl_on\":\"ON\",\"~\":\"espaltherma\",\"q\": 1 }", true);
      client.subscribe("espaltherma/thermMain/set");
#endif

#ifdef PIN_THERM_ADD
      mqttSerial.print("Configuring HA switch for thermAdd");
      client.publish("homeassistant/switch/espAltherma/thermAdd/config", "{" AVAILABILITY "," DEVICE_ID ",\"unique_id\":\"AlthermaAddZone\",\"name\":\"AlthermaAdditionalZone\",\"cmd_t\":\"~/thermAdd/set\",\"stat_t\":\"~/thermAdd/state\",\"pl_off\":\"OFF\",\"pl_on\":\"ON\",\"~\":\"espaltherma\",\"q\": 1 }", true);
      client.subscribe("espaltherma/thermAdd/set");
#endif

#ifdef PIN_SG1
      // Smart Grid
      client.publish("homeassistant/select/espAltherma/sg/config", "{" AVAILABILITY ",\"unique_id\":\"espaltherma_sg\",\"device\":{\"identifiers\":[\"ESPAltherma\"],\"manufacturer\":\"ESPAltherma\",\"model\":\"M5StickC PLUS ESP32-PICO\",\"name\":\"ESPAltherma\"},\"icon\":\"mdi:solar-power\",\"name\":\"EspAltherma Smart Grid\",\"command_topic\":\"espaltherma/sg/set\",\"command_template\":\"{% if value == 'Free Running' %} 0 {% elif value == 'Forced Off' %} 1 {% elif value == 'Recommended On' %} 2 {% elif value == 'Forced On' %} 3 {% else %} 0 {% endif %}\",\"options\":[\"Free Running\",\"Forced Off\",\"Recommended On\",\"Forced On\"],\"state_topic\":\"espaltherma/sg/state\",\"value_template\":\"{% set mapper = { '0':'Free Running', '1':'Forced Off', '2':'Recommended On', '3':'Forced On' } %} {% set word = mapper[value] %} {{ word }}\"}", true);
      client.subscribe("espaltherma/sg/set");
      client.publish("espaltherma/sg/state", "0");
#endif

#ifndef PIN_THERM
      // Publish empty retained message so discovered entities are removed from HA
      client.publish("homeassistant/select/espAltherma/POWER", "", true);
#endif

#ifndef PIN_THERM_MAIN
      // Publish empty retained message so discovered entities are removed from HA
      client.publish("homeassistant/select/espAltherma/thermMain/config", "", true);
#endif

#ifndef PIN_THERM_ADD
      // Publish empty retained message so discovered entities are removed from HA
      client.publish("homeassistant/select/espAltherma/thermAdd/config", "", true);
#endif

#ifndef PIN_SG1
      // Publish empty retained message so discovered entities are removed from HA
      client.publish("homeassistant/select/espAltherma/sg/config", "", true);
#endif
    }
    else
    {
      Serial.printf("failed, rc=%d, try again in 5 seconds", client.state());
      unsigned long start = millis();
      while (millis() < start + 5000)
      {
        ArduinoOTA.handle();
      }

      if (i++ == 100) {
        Serial.printf("Tried for 500 sec, rebooting now.");
        restart_board();
      }
    }
  }
}

#ifdef PIN_THERM
void callbackTherm(byte *payload, unsigned int length)
{
  payload[length] = '\0';

  // Is it ON or OFF?
  // Ok I'm not super proud of this, but it works :p
  if (payload[1] == 'F')
  { //turn off
    digitalWrite(PIN_THERM, HIGH);
    EEPROM.write(EEPROM_STATE,HIGH);
    EEPROM.commit();
    client.publish("espaltherma/STATE", "OFF", true);
    mqttSerial.print("Therm turned OFF");
  }
  else if (payload[1] == 'N')
  { //turn on
    digitalWrite(PIN_THERM, LOW);
    EEPROM.write(EEPROM_STATE,LOW);
    EEPROM.commit();
    client.publish("espaltherma/STATE", "ON", true);
    mqttSerial.print("Therm turned ON");
  }
  else if (payload[0] == 'R')//R(eset/eboot)
  {
    mqttSerial.print("*** Rebooting ***");
    delay(100);
    restart_board();
  }
  else
  {
    Serial.printf("Unknown message: %s\n", payload);
  }
}
#endif

#ifdef PIN_THERM_MAIN
void callbackThermMain(byte *payload, unsigned int length)
{
  payload[length] = '\0';

  // Is it ON or OFF?
  if (payload[1] == 'F')
  { //turn off
    if (digitalRead(PIN_THERM_MAIN) != LOW) {
      digitalWrite(PIN_THERM_MAIN, LOW);
      EEPROM.write(EEPROM_STATE_MAIN,LOW);
      EEPROM.commit();
    }
    mqttSerial.print("Therm-Main turned OFF");
    client.publish("espaltherma/thermMain/state", "OFF", true);
  }
  else if (payload[1] == 'N')
  { //turn on
      if (digitalRead(PIN_THERM_MAIN) != HIGH) {
        digitalWrite(PIN_THERM_MAIN, HIGH);
        EEPROM.write(EEPROM_STATE_MAIN,HIGH);
        EEPROM.commit();
      }
      mqttSerial.print("Therm-Main turned ON");
      client.publish("espaltherma/thermMain/state", "ON",true);
  }
  else
  {
    Serial.printf("Unknown message: %s\n", payload);
  }
}
#endif

#ifdef PIN_THERM_ADD
void callbackThermAdd(byte *payload, unsigned int length)
{
    payload[length] = '\0';

  // Is it ON or OFF?
  if (payload[1] == 'F')
  { //turn off
    if (digitalRead(PIN_THERM_ADD) != LOW) {
      digitalWrite(PIN_THERM_ADD, LOW);
      EEPROM.write(EEPROM_STATE_ADD, LOW);
      EEPROM.commit();
    }
    mqttSerial.print("Therm-Add turned OFF");
    client.publish("espaltherma/thermAdd/state", "OFF", true);
  }
  else if (payload[1] == 'N')
  { //turn on
    if (digitalRead(PIN_THERM_ADD) != HIGH) {
      digitalWrite(PIN_THERM_ADD, HIGH);
      EEPROM.write(EEPROM_STATE_ADD,HIGH);
      EEPROM.commit();
    }
    mqttSerial.print("Therm-Add turned ON");
    client.publish("espaltherma/thermAdd/state", "ON", true);
  }
  else
  {
    Serial.printf("Unknown message: %s\n", payload);
  }
}
#endif

#ifdef PIN_SG1
//Smartgrid callbacks
void callbackSg(byte *payload, unsigned int length)
{
    payload[length] = '\0';

  if (payload[0] == '0')
  {
    // Set SG 0 mode => SG1 = INACTIVE, SG2 = INACTIVE
    digitalWrite(PIN_SG1, SG_RELAY_INACTIVE_STATE);
    digitalWrite(PIN_SG2, SG_RELAY_INACTIVE_STATE);
    client.publish("espaltherma/sg/state", "0");
    Serial.println("Set SG mode to 0 - Normal operation");
  }
  else if (payload[0] == '1')
  {
    // Set SG 1 mode => SG1 = INACTIVE, SG2 = ACTIVE
    digitalWrite(PIN_SG1, SG_RELAY_INACTIVE_STATE);
    digitalWrite(PIN_SG2, SG_RELAY_ACTIVE_STATE);
    client.publish("espaltherma/sg/state", "1");
    Serial.println("Set SG mode to 1 - Forced OFF");
  }
  else if (payload[0] == '2')
  {
    // Set SG 2 mode => SG1 = ACTIVE, SG2 = INACTIVE
    digitalWrite(PIN_SG1, SG_RELAY_ACTIVE_STATE);
    digitalWrite(PIN_SG2, SG_RELAY_INACTIVE_STATE);
    client.publish("espaltherma/sg/state", "2");
    Serial.println("Set SG mode to 2 - Recommended ON");
  }
  else if (payload[0] == '3')
  {
    // Set SG 3 mode => SG1 = ACTIVE, SG2 = ACTIVE
    digitalWrite(PIN_SG1, SG_RELAY_ACTIVE_STATE);
    digitalWrite(PIN_SG2, SG_RELAY_ACTIVE_STATE);
    client.publish("espaltherma/sg/state", "3");
    Serial.println("Set SG mode to 3 - Forced ON");
  }
  else
  {
    Serial.printf("Unknown message: %s\n", payload);
  }
}
#endif

void callback(char *topic, byte *payload, unsigned int length)
{
  char buf[16]= "";
  const int cnt= min(length, sizeof(buf)-1);
  strncpy(buf, (const char*)payload, cnt);
  buf[cnt]= 0;

  char tp[100];
  tp[99]='\0';
  strcpy(tp, topic);

  // mqttSerial.printf("Message arrived [%s, %i]: %s", topic, length, buf);
  
#ifdef PIN_THERM_MAIN
  if (strcmp(tp, "espaltherma/thermMain/set") == 0)
  {
    callbackThermMain(payload, length);
    return;
  }
#endif

#ifdef PIN_THERM_ADD
  if (strcmp(tp, "espaltherma/thermAdd/set") == 0)
  {
    callbackThermAdd(payload, length);
    return;
  }
#endif

#ifdef PIN_THERM
  if (strcmp(tp, "espaltherma/POWER") == 0)
  {
    callbackTherm(payload, length);
    return;
  }
#endif

#ifdef PIN_SG1
  if (strcmp(tp, "espaltherma/sg/set") == 0)
  {
    callbackSg(payload, length);
    return;
  }
#endif
  
  mqttSerial.printf("Unknown topic: %s", tp);
}
