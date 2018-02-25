#include "RampIOTESPShield.h"
#include "ArduinoJson.h"
#include "ESP8266WebServer.h"
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <AsyncMqttClient.h>

#define BAUD_RATE 9600
#define TIMEOUT_AFTER_WS_RESPONSE 5000
#define TIMEOUT_AFTER_WIFI_CONNECT 5000

/** EEPROM SLOTS
0: ssid
1: password
2: userId
3: mqttPass
4: EMPTY
5: thingName
6: EMPTY
7: apSsid
8: apPassword
*/

bool beginned = false;
bool webServerStarted = false;
bool ap_mode = false;
bool tryingToConnectToAP = false;
bool dataAvailable = false;
bool thingRegistered = false;
bool thingLoggedIn = false;
bool attempConnection = false;
bool topicsLoaded = false;

ESP8266WebServer webServer(80);

byte resetInputPin = -1;
byte resetLastStatus = LOW;

char thingId[THING_ID_LENGTH];
char thingPass[THING_PASS_LENGTH];
char thingName[THING_NAME_LENGTH];
char thingType[THING_TYPE_LENGTH];

char _eventTopicPattern[MAX_TOPIC_LENGTH];
char _fireTopicPattern[MAX_TOPIC_LENGTH];
char _aclTopicPattern[MAX_TOPIC_LENGTH];
char _propsUpdateTopicPattern[MAX_TOPIC_LENGTH];
char _notificationTopicPattern[MAX_TOPIC_LENGTH];
char _ownerUpdateTopicPattern[MAX_TOPIC_LENGTH];

unsigned long lastTimeReqReceived = 0;
unsigned long lastTimeWifiConn = 0;

AsyncMqttClient mqttClient;

String jsonResponse;
String Buff = "";

RampIOTESPShield::RampIOTESPShield(){}

void RampIOTESPShield::begin(){
    Serial.begin(BAUD_RATE);
}

bool endsWith (char* base, char* str) {
    int blen = strlen(base);
    int slen = strlen(str);
    return (blen >= slen) && (0 == strcmp(base + blen - slen, str));
}

void __onMessage(
	char* topic, char* payload, AsyncMqttClientMessageProperties properties, 
	size_t len, size_t index, size_t total
	){
	if( endsWith(topic, "/fire") ){
		DynamicJsonBuffer jsonBuffer;
		JsonObject& msg = jsonBuffer.parseObject(payload);
		const char* _id = msg["_id"];
		if( _id && strcmp(_id, thingId) == 0 ){
			DynamicJsonBuffer jBuffer;
			JsonObject& onMessage = jBuffer.createObject();
			onMessage["method"] = "onMessage";
			onMessage["topic"] = topic;
			onMessage["status"] = msg["status"];
			onMessage["fireUserId"] = msg["fireUserId"];
			onMessage.printTo(Serial);
			Serial.println();
		}    
	}
	else if( strcmp(topic, _propsUpdateTopicPattern) == 0 ){
		DynamicJsonBuffer jsonBuffer;
		JsonObject& pl = jsonBuffer.parseObject(payload);
		DynamicJsonBuffer jBuffer;
		JsonObject& onProperties = jBuffer.createObject();
		onProperties["method"] = "onProperties";
		onProperties["properties"] = pl;
		onProperties.printTo(Serial);
		Serial.println();
	}
}

void onMqttConnect(bool sessionPresent) {
	mqttClient.subscribe(_fireTopicPattern, QoS);
	mqttClient.subscribe(_aclTopicPattern, QoS);
	mqttClient.subscribe(_propsUpdateTopicPattern, QoS);
	mqttClient.subscribe(_ownerUpdateTopicPattern, QoS);	
	DynamicJsonBuffer jsonBuffer;
	JsonObject& root = jsonBuffer.parseObject(jsonResponse);
	if( root.success() && root.containsKey("allowedReadTopics") ){
		int size = root["allowedReadTopics"].size();
		size = size > MAX_LISTEN ? MAX_LISTEN : size;
		for( int i=0;i<size;i++ ){
			const char* topic = root["allowedReadTopics"][i];			
			mqttClient.subscribe(topic, QoS);			
		}
	}
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {   	
   	attempConnection = false;	
}

void onBegin(uint32_t tId, const char* _tType){
    itoa (tId, thingId, 10);
    strcpy(thingType, _tType);
	Storage storage;
    storage.begin();
	storage.getData(5, thingName);	
	mqttClient.setServer(THINGS_SERVER, MQTT_PORT);
	mqttClient.onConnect(onMqttConnect);
    mqttClient.onDisconnect(onMqttDisconnect);
	mqttClient.onMessage(__onMessage);	
	thingRegistered = storage.getSlotSize(3) > 0;
	if( thingRegistered ){
		storage.getData(3, thingPass); 
	}
	char buff[storage.getSlotSize(0)+1];
	storage.getData(0, buff);
	ap_mode = strlen(buff) == 0;
    beginned = true;
}

void initializeTopics(JsonObject& root){
	if( !topicsLoaded ){
		const char* __eventTopicPattern = root["eventTopicPattern"];
		const char* __fireTopicPattern = root["fireTopicPattern"];
		const char* __aclTopicPattern = root["aclTopicPattern"];
		const char* __propsUpdateTopicPattern = root["propsUpdateTopicPattern"];
		const char* __notificationTopicPattern = root["notificationTopicPattern"];
		const char* __ownerUpdateTopicPattern = root["ownerUpdateTopicPattern"];
		strcpy(_eventTopicPattern, __eventTopicPattern);
		strcpy(_fireTopicPattern, __fireTopicPattern);
		strcpy(_aclTopicPattern, __aclTopicPattern);
		strcpy(_propsUpdateTopicPattern, __propsUpdateTopicPattern);
		strcpy(_notificationTopicPattern, __notificationTopicPattern);
		strcpy(_ownerUpdateTopicPattern, __ownerUpdateTopicPattern);
		topicsLoaded = true;
	}
}

boolean isValidParameter(char *param){
  return webServer.hasArg(param) && webServer.arg(param).length() > 3;
}

void turnOnWifiSTAMode(){
	ap_mode = false;
	tryingToConnectToAP = false;
}

void afterSuccessWSResponse(){
    turnOnWifiSTAMode();
}

void handleWifiConfig(){
  if( isValidParameter("ssid") && isValidParameter("password") && isValidParameter("name") 
  		&& isValidParameter("userId") ){
    lastTimeReqReceived = millis();
    webServer.send(200, FPSTR(APP_JSON_CONTENT_TYPE), FPSTR(CONFIG_OK_RESPONSE));	
    Storage storage;
	storage.begin();        
    storage.saveStringData(0, webServer.arg("ssid"));
    storage.saveStringData(1, webServer.arg("password"));
	storage.saveStringData(2, webServer.arg("userId"));	
    storage.saveStringData(5, webServer.arg("name"));
	webServer.arg("name").toCharArray(thingName, sizeof(thingName));
  }else{
    webServer.send(200, FPSTR(APP_JSON_CONTENT_TYPE), FPSTR(CONFIG_ERROR_RESPONSE));
  }
}

void handleWifiInfo(){
	Storage storage;
   	storage.begin();
	const size_t bufferSize = JSON_OBJECT_SIZE(6) + 210;
	StaticJsonBuffer<bufferSize> jsonBuffer;
	JsonObject& info = jsonBuffer.createObject();	
	char ssid_buff[32];
	storage.getData(0, ssid_buff);
	char uid_buff[32];
	storage.getData(2, uid_buff);
	char mqttpass_buff[32];
	storage.getData(3, mqttpass_buff);
	info["ssid"] = ssid_buff;
	info["userId"] = uid_buff;
	info["_id"] = thingId;
	info["name"] = thingName;
	info["type"] = thingType;
	info["mqttPass"] = mqttpass_buff;
	if( info.success() ){		
		char _buffer[info.measureLength()+1];
		info.printTo(_buffer, sizeof(_buffer));
   		webServer.send(200, FPSTR(APP_JSON_CONTENT_TYPE), _buffer);
	}	
}

void turnOnWifiAPMode(){
	Storage storage;
	storage.begin();
	if( storage.getSlotSize(7) == 0 ){
		String ap_ssid = FPSTR(AP_SSID_PREFIX);
		String ap_pass = FPSTR(DEFAULT_AP_PASSWORD);
		ap_ssid += String(thingId);
		char ssid_buff[ap_ssid.length()+1];
		ap_ssid.toCharArray(ssid_buff, sizeof(ssid_buff));
		storage.saveData(7, ssid_buff);
		storage.saveStringData(8, ap_pass);
	}
	WiFi.disconnect();  	  	
  	char ap_ssid_buff[25];
  	char ap_pass_buff[20];  	
  	storage.getData(7, ap_ssid_buff);
  	storage.getData(8, ap_pass_buff);
  	WiFi.softAP(ap_ssid_buff, ap_pass_buff);
	WiFi.mode(WIFI_AP);	
}

void handleWifi(){	
	if( ap_mode ){        
		if( !webServerStarted ){
			turnOnWifiAPMode();
			webServer.on("/endpoint", handleWifiConfig);
			webServer.on("/info", handleWifiInfo);
			webServer.begin();
			webServerStarted = true;
		}
		webServer.handleClient();
	}
	else if( !tryingToConnectToAP ) {
		WiFi.disconnect();
		Storage storage;
	  	storage.begin();
		char ssid_buff[25];
		char pass_buff[20];
		storage.getData(0, ssid_buff);
		storage.getData(1, pass_buff);	
		if( strlen(ssid_buff) > 0 && strlen(pass_buff) > 0 ){
			WiFi.begin(ssid_buff, pass_buff);
			WiFi.mode(WIFI_STA);
			tryingToConnectToAP = true;
		}
	}	
}

void handleRegister(){
	if ( WiFi.status() == WL_CONNECTED && strlen(thingId) > 0 && !thingRegistered ){
		Storage storage;
	  	storage.begin();
		char userId[USER_ID_LENGTH];
		storage.getData(2, userId);		
		HTTPClient httpClient;
		httpClient.begin(FPSTR(SIGNUP_URL), FPSTR(FINGERPRINT));
		httpClient.addHeader("Content-Type", FPSTR(APP_JSON_CONTENT_TYPE));
		const int REQ_BUFFER_SIZE = JSON_OBJECT_SIZE(6);
		StaticJsonBuffer<REQ_BUFFER_SIZE> jsonBufferReq;
		JsonObject& signup_body = jsonBufferReq.createObject();
		signup_body["_id"] = thingId; 
		signup_body["name"] = thingName;
		signup_body["type"] = thingType;
		signup_body["userId"] = userId;
		signup_body["maxListen"] = MAX_LISTEN;
		char _buffer[signup_body.measureLength()+1];
		signup_body.printTo(_buffer, sizeof(_buffer));
		int httpCode = httpClient.POST(_buffer);
		if (httpCode > 0) {
			if (httpCode == HTTP_CODE_OK) {
				jsonResponse = httpClient.getString();
				const size_t bufferSize = JSON_ARRAY_SIZE(2) + JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(15) + 670;
				DynamicJsonBuffer jsonBuffer(bufferSize);
				JsonObject& root = jsonBuffer.parseObject(jsonResponse);
				if (root.success()) {
					const char* _broker_pass = root["broker_pass"];
					storage.saveData(3, _broker_pass);
					storage.getData(3, thingPass);					
					initializeTopics(root);					
					thingRegistered = true;
					thingLoggedIn = true;
					JsonObject& properties = root["properties"];
					if( properties.success() ){
						DynamicJsonBuffer jBuffer;
						JsonObject& props = jsonBuffer.createObject();
						props["method"] = "onProperties";
						props["properties"] = properties;
						props.printTo(Serial);
						Serial.println();
					}
				}
			}else{
				delay(DELAY_AFTER_FAIL);
			}
		} else {
			delay(DELAY_AFTER_FAIL);
		}
    	httpClient.end();		
	}
}

void handleLogin(){
	if ( WiFi.status() == WL_CONNECTED && strlen(thingId) > 0 && thingRegistered && !thingLoggedIn ){	
		HTTPClient httpClient;
		httpClient.begin(FPSTR(LOGIN_URL), FPSTR(FINGERPRINT));
		httpClient.addHeader("Content-Type", FPSTR(APP_JSON_CONTENT_TYPE));		
		const size_t bufferSize = JSON_OBJECT_SIZE(2) + 40;
		StaticJsonBuffer<bufferSize> jsonBufferReq;
		JsonObject& login_body = jsonBufferReq.createObject();
		login_body["thingId"] = thingId;
		login_body["pass"] = thingPass;		
		char _buffer[login_body.measureLength()+1];
		login_body.printTo(_buffer, sizeof(_buffer));
		int httpCode = httpClient.POST(_buffer);
		if (httpCode > 0) {
			if (httpCode == HTTP_CODE_OK) {
				jsonResponse = httpClient.getString();
				const size_t bufferSize = JSON_ARRAY_SIZE(2) + JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(15) + 670;
				DynamicJsonBuffer jsonBuffer(bufferSize);
				JsonObject& root = jsonBuffer.parseObject(jsonResponse);
				if (root.success()) {
					initializeTopics(root);					
					thingLoggedIn = true;
					JsonObject& properties = root["properties"];
					if( properties.success() ){
						DynamicJsonBuffer jBuffer;
						JsonObject& props = jsonBuffer.createObject();
						props["method"] = "onProperties";
						props["properties"] = properties;
						props.printTo(Serial);
						Serial.println();
					}
				}
			}else{
				delay(DELAY_AFTER_FAIL);
			}
		}else{
			delay(DELAY_AFTER_FAIL);
		}
		httpClient.end();		
	}	
}

void handleMQTT(){
	if( WiFi.status() == WL_CONNECTED && !mqttClient.connected() && !attempConnection && thingRegistered && thingLoggedIn ){
      mqttClient.setKeepAlive(6).
      setCredentials(thingId, thingPass).
      setClientId(thingId);	  
      mqttClient.connect();
      attempConnection = true;
    }
}

void onReset(){
	Storage storage;
	storage.begin();
	storage.clearAll();
	thingRegistered = false;
	thingLoggedIn = false;
	attempConnection = false;
	ap_mode = true;
	tryingToConnectToAP = false;
	turnOnWifiAPMode();			
	Serial.println("{\"method\":\"onReset\"}");
}

void publishEvent(JsonObject& status, const char* fireUserId){
    if( WiFi.status() == WL_CONNECTED && thingRegistered && mqttClient.connected() ){
        DynamicJsonBuffer jsonBuffer;
        JsonObject& root = jsonBuffer.createObject();
        root["_id"] = thingId;
        root["type"] = thingType;
        root["name"] = thingName;
        root["fireUserId"] = fireUserId;
        root["status"] = status;
        char _buffer[root.measureLength()+1];
        root.printTo(_buffer, sizeof(_buffer));
        mqttClient.publish(_eventTopicPattern, QoS, false, _buffer);		
    }
}

void RampIOTESPShield::handleShield(){	
    while(Serial.available()) {
        char data = Serial.read();
        if( data == '\n' ){
            dataAvailable = true;
        }else{
            Buff.concat(data);
        }
    }
    if( dataAvailable ){
        dataAvailable = false;
        DynamicJsonBuffer jsonBuffer;
        char buff[Buff.length()+1];
        Buff.toCharArray(buff, Buff.length()+1);            
        JsonObject& root = jsonBuffer.parseObject(buff);
        Buff = "";
        if (root.success()) {                
            const char* _method = root["method"];
            if( strcmp_P(_method, "begin") == 0 ){
                uint32_t thingId = root["thingId"];
                const char* thingType = root["thingType"];
                onBegin(thingId, thingType);
            }
			else if( strcmp_P(_method, "reset") == 0 ){
                onReset();
            }
			else if( strcmp_P(_method, "publishEvent") == 0 ){
				JsonObject& status = root["status"];
				const char* fireUserId = root["fireUserId"];
				publishEvent(status, fireUserId);
			}
        }
    }    
    if( beginned ){
        handleWifi();
		if( WiFi.status() == WL_CONNECTED && lastTimeWifiConn == 0 ){
			lastTimeWifiConn = millis();
		}
        if( lastTimeReqReceived > 0 && millis()-lastTimeReqReceived > TIMEOUT_AFTER_WS_RESPONSE ){
            lastTimeReqReceived = 0;
            afterSuccessWSResponse();
        }
        if( !ap_mode && millis()-lastTimeWifiConn > TIMEOUT_AFTER_WIFI_CONNECT ){
            handleLogin();
            handleRegister();
            handleMQTT();
        }        
    }	
	delay(LOOP_DELAY);
}