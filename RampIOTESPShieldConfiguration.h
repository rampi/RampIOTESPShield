#ifndef RampIOTESPShieldConfiguration_H
#define RampIOTESPShieldConfiguration_H

#define THING_PASS_LENGTH 8
#define THING_ID_LENGTH 11
#define THING_TYPE_LENGTH 15
#define THING_NAME_LENGTH 30
#define USER_ID_LENGTH 11
#define THINGS_SERVER "mqtt.rampiot.com"
#define MQTT_PORT 8883
#define QoS 0
#define MAX_LISTEN 4
#define DELAY_AFTER_FAIL 3000
#define LOOP_DELAY 100
#define MAX_TOPIC_LENGTH 60

const char CONFIG_OK_RESPONSE[] PROGMEM     = "{\"code\":200, \"status\": \"OK\"}";
const char CONFIG_ERROR_RESPONSE[] PROGMEM  = "{\"code\":500, \"status\": \"ERROR\"}";
const char APP_JSON_CONTENT_TYPE[] PROGMEM  = "application/json";
const char DEFAULT_AP_PASSWORD[] PROGMEM    = "12345678910";
const char TOPIC_PREFIX[] PROGMEM           = "rampiot/";
const char AP_SSID_PREFIX[] PROGMEM	        = "RAMPIOT";
const char SIGNUP_URL[] PROGMEM		        = "https://api.rampiot.com/v1/things/signup";
const char LOGIN_URL[] PROGMEM		        = "https://api.rampiot.com/v1/things/login";
const char FINGERPRINT[] PROGMEM 	        = "A7 9B 55 C7 6E 21 6B 21 D3 3A ED 59 8A 10 57 16 AE C2 57 2B";

#endif