#include <Wire.h>
#include "DHT.h"
#include "Adafruit_ADS1X15.h"
#include "string.h"
#include <Preferences.h>

#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>

#include <ArduinoJson.h>
#include "ESP32HTTP.h"

//define sound speed in cm/uS
#define SOUND_SPEED 0.034
#define FINAL_CHAR '~'

#define PREFERENCES_FILE "myfile"
#define PREFERENCES_SSID "p_wifi_ssid"
#define PREFERENCES_SSID_PASSWORD "p_wifi_pass"
#define PREFERENCES_TANK_HEIGHT "p_tank_height"

#define LED_BUILTIN 2

String html_code = "<!DOCTYPE html>\n" 
"<html>\n" 
"<head>\n" 
"  <meta charset=\"utf-8\">\n" 
" <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n" 
" <title>Irrigatron</title>\n" 
" <style>\n" 
"   body {\n" 
"     font-family: Arial, sans-serif;\n" 
"     background-color: #f0f0f0;\n" 
"     margin: 0;\n" 
"     padding: 0;\n" 
"     display: flex;\n" 
"     justify-content: center;\n" 
"     align-items: center;\n" 
"     height: 100vh;\n" 
"   }\n" 
"   \n" 
"   form {\n" 
"     background-color: #fff;\n" 
"     border-radius: 8px;\n" 
"     padding: 20px;\n" 
"     box-shadow: 0px 2px 6px rgba(0, 0, 0, 0.1);\n" 
"     width: 300px;\n" 
"   }\n" 
"   \n" 
"   input {\n" 
"     width: 93%;\n" 
"     padding: 10px;\n" 
"     margin-bottom: 15px;\n" 
"     border: 1px solid #ccc;\n" 
"     border-radius: 4px;\n" 
"     font-size: 16px;\n" 
"     transition: border-color 0.3s ease-in-out;\n" 
"   }\n" 
"   \n" 
"   input:focus {\n" 
"     border-color: #007bff;\n" 
"   }\n" 
"   \n" 
"   input::placeholder {\n" 
"     color: #999;\n" 
"   }\n" 
"   \n" 
"   input[type=\"submit\"] {\n" 
"     background-color: #007bff;\n" 
"     width: 100%;\n" 
"     color: #fff;\n" 
"     border: none;\n" 
"     cursor: pointer;\n" 
"     transition: background-color 0.3s ease-in-out;\n" 
"   }\n" 
"   \n" 
"   input[type=\"submit\"]:hover {\n" 
"     background-color: #0056b3;\n" 
"   }\n" 
"   h1 {\n" 
"            font-size: 24px;\n" 
"            text-align: center;\n" 
"            margin-bottom: 15px;\n" 
"        }\n" 
" </style>\n" 
"</head>\n" 
"<body>\n" 
" <form method=\"GET\" action=\"/config\">\n" 
"   <h1>Irrigatron Configuration</h1>\n" 
"   <input type=\"text\" placeholder=\"WiFi name\" name=\"ssid\" required>\n" 
"   <input type=\"password\" placeholder=\"WiFi password\" name=\"pass\" required>\n" 
"   <input type=\"number\" placeholder=\"Tank height\" name=\"tank\" step=\"0.1\" min=\"0\" required>\n" 
"   <input type=\"submit\" value=\"Submit\">\n" 
" </form>\n" 
"</body>\n" 
"</html>\n";


//Pin definition
const int dht1Pin = 27;
const int dht2Pin = 26;
const int latchPin = 33;
const int clockPin = 32;
const int dataPin = 25;
const int trigPin = 12;
const int echoPin = 14;
const int configPin = 18;


//configurables
String configurable_wifi_ssid = "";
String configurable_wifi_pass = "";
float configurable_tank_height = 0.0;


//globales
bool server_available = true;
char* server = "192.168.1.138";
String payload_to_server = "";
bool pump_state[6] = {false, false, false, false, false, false};

HTTP https(server, HTTP_PORT, false);  //server, port, debug
//HTTPS https(server, HTTPS_PORT, rootCA, false);  //server, port, rootCA, debug
HTTPRequest httpRequest = HTTPRequest();
HTTPResponse httpResponse = HTTPResponse();


//objects
DHT dht1(dht1Pin, DHT22);
DHT dht2(dht2Pin, DHT22);
Adafruit_ADS1115 ads1;
Adafruit_ADS1115 ads2;

WiFiServer wifiServer(80);
Preferences preferences;

//functions
bool checkConfig();
void logMessage(String s);
bool readADS(uint8_t ads_index, uint8_t ads_pin, int16_t *adc, float *voltage);
bool readDHT(uint8_t dht_index, float *temperature, float *humidity);
float readDistance();
int I2CScanner();
void turnPumps(bool pump1 = false, bool pump2 = false, bool pump3 = false, bool pump4 = false, bool pump5 = false, bool pump6 = false);

void setup() {
  
  Serial.begin(9600);
  Serial.println("");
  
  logMessage("Iniciating program");

  //Inicialization
  logMessage("Iniciating sensors");
  Wire.begin();
  dht1.begin();
  dht2.begin();
  ads1.begin(0x48);
  ads2.begin(0x49);
  preferences.begin(PREFERENCES_FILE, false);
  
  //Pin declaration
  logMessage("Declaring pins");
  pinMode(latchPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(dataPin, OUTPUT);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(configPin, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  //Turning off pumps
  turnPumps(false, false, false, false, false, false);

  int i2c_devices = I2CScanner();
  if(i2c_devices != 2){
    logMessage("Error with i2c_devices");
    ESP.restart();
  }

  //Check if config  
  if(checkConfig()){
    logMessage("Configuration enabled");

    logMessage("WiFi>\tInicializating WiFi");
    if (!WiFi.softAP("irrigatron", "irrigatron")) {
      logMessage("WiFi>\tSoft AP creation failed.");
      while(1);
    }
    IPAddress myIP = WiFi.softAPIP();
    logMessage("WiFi>\tAP IP address: " + String(myIP));
    wifiServer.begin();
  
    logMessage("WiFi>\tServer started");
    
    String information = "";
    while(checkConfig()){
      WiFiClient client = wifiServer.available();   // listen for incoming clients
      if (client) {                             // if you get a client,
        logMessage("WiFi>\tNew Client.");           // print a message out the serial port
        String currentLine = "";                // make a String to hold incoming data from the client
        while (client.connected()) {            // loop while the client's connected
          if (client.available()) {             // if there's bytes to read from the client,
            char c = client.read();             // read a byte, then
            //Serial.write(c);                    // print it out the serial monitor
            if (c == '\n') {                    // if the byte is a newline character
    
              // if the current line is blank, you got two newline characters in a row.
              // that's the end of the client HTTP request, so send a response:
              if (currentLine.length() == 0) {
                // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
                // and a content-type so the client knows what's coming, then a blank line:
                client.println("HTTP/1.1 200 OK");
                client.println("Content-type:text/html");
                client.println();
                
                // the content of the HTTP response follows the header:
                client.print(html_code);
    
                // The HTTP response ends with another blank line:
                client.println();
                // break out of the while loop:
                break;
              } else {    // if you got a newline, then clear currentLine:
                currentLine = "";
              }
            } else if (c != '\r') {  // if you got anything else but a carriage return character,
              currentLine += c;      // add it to the end of the currentLine
            }
    
            // Check to see if the client request was "GET /H" or "GET /L":
            if (currentLine.startsWith("GET /config")) {
              information = currentLine.substring(currentLine.indexOf("?")+1, currentLine.indexOf(" ", currentLine.indexOf("?")+1));
              logMessage("WiFi>\t" + String(information));
            }
          }
        }
        // close the connection:
        client.stop();
        logMessage("WiFi>\tClient Disconnected.");
      }      
      delay(20);
    }
    if(information.length() > 0){
      logMessage("WiFi>\t" + String(information));
      String configurable_wifi_ssid = "";
      if(information.indexOf("ssid=") != -1){
        configurable_wifi_ssid = information.substring(information.indexOf("ssid=")+5, information.indexOf("&", information.indexOf("ssid=")));
      }
      String configurable_wifi_pass = "";
      if(information.indexOf("pass=") != -1){
        configurable_wifi_pass = information.substring(information.indexOf("pass=")+5, information.indexOf("&", information.indexOf("pass=")));
      }
      String configurable_tank_height_str = "";
      if(information.indexOf("tank=") != -1){
        configurable_tank_height_str = information.substring(information.indexOf("tank=")+5, information.indexOf("&", information.indexOf("tank=")));
      }
      float configurable_tank_height = configurable_tank_height_str.toFloat();
  
      logMessage("WiFi>\tssid" + String(configurable_wifi_ssid));
      logMessage("WiFi>\tpass" + String(configurable_wifi_pass));
      logMessage("WiFi>\ttank" + String(configurable_tank_height));
      
      logMessage("WiFi>\tSetting new parameters");
      preferences.putString(PREFERENCES_SSID, configurable_wifi_ssid);
      preferences.putString(PREFERENCES_SSID_PASSWORD, configurable_wifi_pass);
      preferences.putFloat(PREFERENCES_TANK_HEIGHT, configurable_tank_height);
    }
    //restart for apply changes
    logMessage("Restaring Irrigatron");
    ESP.restart();
  }
  

  //getting configurables from EEPROM
  logMessage("Getting configurables from EEPROM");
  configurable_wifi_ssid = preferences.getString(PREFERENCES_SSID, "");
  configurable_wifi_pass = preferences.getString(PREFERENCES_SSID_PASSWORD, "");
  configurable_tank_height = preferences.getFloat(PREFERENCES_TANK_HEIGHT, 0.1);

  if(configurable_tank_height <= 0){
    configurable_tank_height = 0.1;
  }

  if(configurable_wifi_ssid == ""){
    logMessage("irrigatron not configured");
    ESP.restart();
  } else {
    logMessage("WIFI >\tConnecting to " + String(configurable_wifi_ssid));
    WiFi.begin(configurable_wifi_ssid.c_str(), configurable_wifi_pass.c_str());

    int counter = 0;
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      /*
        0: WL_IDLE_STATUS when Wi-Fi is changing state
        1: WL_NO_SSID_AVAIL if the configured SSID cannot be reached
        3: WL_CONNECTED after connection is established
        4: WL_CONNECT_FAILED if the password is incorrect
        6: WL_DISCONNECTED if the module is not configured in station mode
      */
      Serial.print(WiFi.status());
      counter++;
      
      if (counter >= 60) {  //after 30 seconds timeout - reset board
        Serial.println();
        logMessage("WIFI >\tcannot connect to wifi. Rebooting...");
        ESP.restart();
      }
    }
    Serial.println();
  }

  logMessage("WIFI >\tConnected");
  Serial.print("WIFI>\tLocalIP: ");
  Serial.println(WiFi.localIP());
}



void loop() {
  long timestamp = millis();
  long seconds = timestamp/1000;

  if(server_available){
    if(seconds%67 == 0){
      String json = readAlltoJSON();
  
      logMessage("JSON >\t" + json);
  
      sendData(json); 
    } 
  } else {
      if(seconds%149 == 0){
        serverTest();
      }
  }

  if(seconds%29 == 0){
    if(server_available){
      getPumpOrder();
      logMessage("Checking Pumps");
      if(pump_state[0] == true || pump_state[1] == true || pump_state[2] == true || 
          pump_state[3] == true || pump_state[4] == true || pump_state[5] == true){
        logMessage("Some pump must turn on");
        sendWatering();
      }
    }
    turnPumps(pump_state[0], pump_state[1], pump_state[2], pump_state[3], pump_state[4], pump_state[5]);
    for(int i = 0; i < 6; i++){
      pump_state[i] = false;
    }
    delay(1025); //para que no pille dos veces la misma función por el seconds%29
  }
}

bool checkConfig(){
  return !digitalRead(configPin);
}

int I2CScanner(){
  int nDevices = 0;
  logMessage("I2C Scanning...");

  for (byte address = 1; address < 127; ++address) {
    // The i2c_scanner uses the return value of
    // the Write.endTransmisstion to see if
    // a device did acknowledge to the address.
    Wire.beginTransmission(address);
    byte error = Wire.endTransmission();
  
    if (error == 0) {
      logMessage("I2C >\tDevice found at address 0x" + String(address,HEX));
      ++nDevices;
    } else if (error == 4) {
      logMessage("I2C >\tUnknown error at address 0x" + String(address,HEX));
    }
  }
  logMessage("I2C > Devices: " + String(nDevices));
  return nDevices;
}

void logMessage(String s){
  Serial.print(F("(LOG)\t"));
  Serial.println(s);
}


void turnPumps(bool pump1, bool pump2, bool pump3, bool pump4, bool pump5, bool pump6){
  int num = 0;
  if(pump1){
    logMessage("Pump 1 >\tTurning on");
    num = 1;
  }
  if(pump2){
    logMessage("Pump 2 >\tTurning on");
    num += 2;
  }
  if(pump3){
    logMessage("Pump 3 >\tTurning on");
    num += 4;
  }
  if(pump4){
    logMessage("Pump 4 >\tTurning on");
    num += 32;
  }
  if(pump5){
    logMessage("Pump 5 >\tTurning on");
    num += 64;
  }
  if(pump6){
    logMessage("Pump 6 >\tTurning on");
    num += 128;
  }
  
  digitalWrite(latchPin, LOW);  //let me modify
  shiftOut(dataPin, clockPin, MSBFIRST, num);
  digitalWrite(latchPin, HIGH); //set the modification
}

String readAlltoJSON(){
  float tank_height = readDistance();
  
  float temperature1 = 0.0, humidity1 = 0.0;
  readDHT(1, &temperature1, &humidity1);
  float temperature2 = 0.0, humidity2 = 0.0;
  readDHT(2, &temperature1, &humidity1);
  
  float soil1 = 0.0, soil2 = 0.0, soil3 = 0.0, soil4 = 0.0, soil5 = 0.0, soil6 = 0.0;
  int16_t soil_int1 = 0, soil_int2 = 0, soil_int3 = 0, soil_int4 = 0, soil_int5 = 0, soil_int6 = 0;
  readADS(2, 0, &soil_int1, &soil1);
  readADS(2, 1, &soil_int2, &soil2);
  readADS(2, 2, &soil_int3, &soil3);
  readADS(2, 3, &soil_int4, &soil4);
  
  float light = 0.0;
  int16_t light_int = 0;
  readADS(1, 0, &light_int, &light);
  readADS(1, 2, &soil_int5, &soil5);
  readADS(1, 3, &soil_int6, &soil6);
  
  String json = "{\"datas\":[";
  json += "{\"soil2\":\""+String(soil2,2)+"\"},";
  json += "{\"soil3\":\""+String(soil3,2)+"\"},";
  json += "{\"soil1\":\""+String(soil1,2)+"\"},";
  json += "{\"soil4\":\""+String(soil4,2)+"\"},";
  json += "{\"soil5\":\""+String(soil5,2)+"\"},";
  json += "{\"soil6\":\""+String(soil6,2)+"\"},";
  json += "{\"soil_int1\":\""+String(soil_int1)+"\"},";
  json += "{\"soil_int2\":\""+String(soil_int2)+"\"},";
  json += "{\"soil_int3\":\""+String(soil_int3)+"\"},";
  json += "{\"soil_int4\":\""+String(soil_int4)+"\"},";
  json += "{\"soil_int5\":\""+String(soil_int5)+"\"},";
  json += "{\"soil_int6\":\""+String(soil_int6)+"\"},";
  json += "{\"temperature1\":\""+String(temperature1,2)+"\"},";
  json += "{\"temperature2\":\""+String(temperature2,2)+"\"},";
  json += "{\"humidity1\":\""+String(humidity1,2)+"\"},";
  json += "{\"humidity2\":\""+String(humidity2,2)+"\"},";
  json += "{\"tank_height\":\""+String(tank_height,2)+"\"},";
  json += "{\"light\":\""+String(light,2)+"\"},";
  json += "{\"light_int\":\""+String(light_int)+"\"}";
  json += "]}";

  return json;
}


bool readADS(uint8_t ads_index, uint8_t ads_pin, int16_t *adc, float *voltage){
  *adc = 0;
  if(ads_index == 1){
    *adc = ads1.readADC_SingleEnded(ads_pin);
  } else {
    *adc = ads2.readADC_SingleEnded(ads_pin);
  }
  *voltage = (*adc * 0.1875)/1000;
  logMessage("ADC: " + String(ads_index) + " " + String(ads_pin) + " >\t" + String(*adc) + "\t" + String(*voltage));

  return true;
}

bool readDHT(uint8_t dht_index, float *temperature, float *humidity){
  if(dht_index == 1){
    *temperature = dht1.readTemperature();
    *humidity = dht1.readHumidity();
  } else {
    *temperature = dht2.readTemperature();
    *humidity = dht2.readHumidity();
  }

  // Check if any reads failed and exit early (to try again).
  if (isnan(*humidity) || isnan(*temperature)) {
    *temperature = 0.0;
    *humidity = 0.0;
    logMessage("DHT: " + String(dht_index) + ">\tFailed to read from DHT sensor");
    return false;
  }

  logMessage("DHT: " + String(dht_index) + " >\t" + String(*temperature) + "\t" + String(*humidity));
  return true;
}

float readDistance(){
  long duration = 0;
  float distance_cm = 0.0;

  // Clears the trigPin
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  // Sets the trigPin on HIGH state for 10 micro seconds
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  // Reads the echoPin, returns the sound wave travel time in microseconds
  duration = pulseIn(echoPin, HIGH);
  
  // Calculate the distance
  distance_cm = duration * SOUND_SPEED/2;
  
  // Prints the distance in the Serial Monitor
  logMessage("Ultrasonic: " + String(distance_cm));
  return distance_cm;
}

void connectToWiFi() {
  uint8_t counter1 = 0;
  uint8_t counter2 = 0;
  while (WiFi.status() != WL_CONNECTED && counter1 < 5) {
    WiFi.disconnect();
    delay(5000);
    WiFi.disconnect();
    delay(5000);
    logMessage("WIFI >\tConnecting to " + String(configurable_wifi_ssid));
    WiFi.begin(configurable_wifi_ssid.c_str(), configurable_wifi_pass.c_str());
    counter2 = 0;
    while (WiFi.status() != WL_CONNECTED && counter2 < 60) {
      delay(500);
      /*
        0: WL_IDLE_STATUS when Wi-Fi is changing state
        1: WL_NO_SSID_AVAIL if the configured SSID cannot be reached
        3: WL_CONNECTED after connection is established
        4: WL_CONNECT_FAILED if the password is incorrect
        6: WL_DISCONNECTED if the module is not configured in station mode
      */
      Serial.print(WiFi.status());
      counter2++;
    }
    Serial.println();
    counter1++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    logMessage("WIFI >\tConnected");
  } else {
    logMessage("WIFI >\tcannot connect to wifi. Rebooting...");
    ESP.restart();
  }
}

unsigned int sendData(String json_payload) {
  unsigned int responseCode = 0;

  if(server_available){
    logMessage("REST >\tUpload");

    logMessage("REST >\tPayload length: " + String(json_payload.length()));

    if (WiFi.status() == WL_CONNECTED) {  //Check the current connection status
      httpRequest = HTTPRequest(HTTP_POST, "/upload", json_payload);
      httpRequest.setHeader("User-Agent", "irrigatron");
      httpRequest.setHeader("Content-Type", "application/json");
      httpResponse = https.sendRequest(httpRequest);

      responseCode = httpResponse.getResponseCode();
      String responsePayload = httpResponse.getPayload();
      
      logMessage("REST >\tResponse Code: "+ String(responseCode));
      logMessage("REST >\tResponse Payload: "+ responsePayload);
      if(responseCode == HTTP_RESPONSE_OK){
        logMessage("REST >\tData sended");
      } else if(responseCode == HTTP_RESPONSE_CONFLICT){
        logMessage("REST >\tSome of the payload send exists in the server");
      } else if(responseCode == HTTP_RESPONSE_PARTIAL_CONTENT){
        logMessage("REST >\tSome of the payload sent is not valid");
      } else {
        logMessage("REST >\tError");
      }
    } else {
      logMessage("REST >\tNot connected to WiFi.");
      connectToWiFi();
    }
    //un código de respuesta 0 significará que el servidor no está disponible
    if(responseCode == 0){
      server_available = false;
    }
  } else {
    logMessage("REST >\tServer is not available");
  }

  return responseCode;
}

unsigned int sendWatering(){

  unsigned int responseCode = 0;

  if(server_available){
    logMessage("REST >\tWatering");
  
    if (WiFi.status() == WL_CONNECTED) {  //Check the current connection status
      String json = "{";
      json += "\"pump0\":\""+String(pump_state[0])+"\",";
      json += "\"pump1\":\""+String(pump_state[1])+"\",";
      json += "\"pump2\":\""+String(pump_state[2])+"\",";
      json += "\"pump3\":\""+String(pump_state[3])+"\",";
      json += "\"pump4\":\""+String(pump_state[4])+"\",";
      json += "\"pump5\":\""+String(pump_state[5])+"\"";
      json += "}";
      
      httpRequest = HTTPRequest(HTTP_POST, "/watering", "{}");
      httpRequest.setHeader("User-Agent", "irrigatron");
      httpRequest.setHeader("Content-Type", "application/json");
      httpResponse = https.sendRequest(httpRequest);

      responseCode = httpResponse.getResponseCode();
      String responsePayload = httpResponse.getPayload();
      
      logMessage("REST >\tResponse Code: "+ String(responseCode));
      logMessage("REST >\tResponse Payload: "+ responsePayload);
      if(responseCode == HTTP_RESPONSE_OK){
        logMessage("REST >\tData sended");
      } else if(responseCode == HTTP_RESPONSE_CREATED){
        logMessage("REST >\tData sended");
      } else if(responseCode == HTTP_RESPONSE_CONFLICT){
        logMessage("REST >\tSome of the payload send exists in the server");
      } else if(responseCode == HTTP_RESPONSE_PARTIAL_CONTENT){
        logMessage("REST >\tSome of the payload sent is not valid");
      } else {
        logMessage("REST >\tCode: " + String(responseCode));
      }
    } else {
      logMessage("REST >\tNot connected to WiFi.");
      connectToWiFi();
    }
    //un código de respuesta 0 significará que el servidor no está disponible
    if(responseCode == 0){
      server_available = false;
    }
  } else {
    logMessage("REST >\tServer is not available");
  }

  return responseCode;
}

unsigned int getPumpOrder() {

  unsigned int responseCode = 0;

  if(server_available){
    logMessage("REST >\tPumpOrder");

    if (WiFi.status() == WL_CONNECTED) {  //Check the current connection status
      httpRequest = HTTPRequest(HTTP_POST, "/water", "{}");
      httpRequest.setHeader("User-Agent", "irrigatron");
      httpRequest.setHeader("Content-Type", "application/json");
      httpResponse = https.sendRequest(httpRequest);

      responseCode = httpResponse.getResponseCode();
      String responsePayload = httpResponse.getPayload();
      
      logMessage("REST >\tResponse Code: "+ String(responseCode));
      logMessage("REST >\tResponse Payload: "+ responsePayload);
      if(responseCode == HTTP_RESPONSE_OK || responseCode == HTTP_RESPONSE_CREATED){
        logMessage("REST >\tData sended");
        //obtengo el JSON y lo deserializo
        DynamicJsonDocument doc(1024);
        deserializeJson(doc, responsePayload);
        Serial.println((bool)doc["pump0"]);
        pump_state[0] = (bool)doc["pump0"];
        pump_state[1] = (bool)doc["pump1"];
        pump_state[2] = (bool)doc["pump2"];
        pump_state[3] = (bool)doc["pump3"];
        pump_state[4] = (bool)doc["pump4"];
        pump_state[5] = (bool)doc["pump5"];
      } else if(responseCode == HTTP_RESPONSE_CONFLICT){
        logMessage("REST >\tSome of the payload send exists in the server");
      } else if(responseCode == HTTP_RESPONSE_PARTIAL_CONTENT){
        logMessage("REST >\tSome of the payload sent is not valid");
      } else {
        logMessage("REST >\tCode: " + String(responseCode));
      }
    } else {
      logMessage("REST >\tNot connected to WiFi.");
      connectToWiFi();
    }
    //un código de respuesta 0 significará que el servidor no está disponible
    if(responseCode == 0){
      server_available = false;
    }
  } else {
    logMessage("REST >\tServer is not available");
  }

  return responseCode;
}

bool serverTest(){
  unsigned int responseCode = 0;

  if(server_available){
    logMessage("REST >\tUpload");

    if (WiFi.status() == WL_CONNECTED) {  //Check the current connection status
      httpRequest = HTTPRequest(HTTP_GET, "/test", "");
      httpRequest.setHeader("User-Agent", "irrigatron");
      httpRequest.setHeader("Content-Type", "application/json");
      httpResponse = https.sendRequest(httpRequest);

      responseCode = httpResponse.getResponseCode();
      String responsePayload = httpResponse.getPayload();
      
      logMessage("REST >\tResponse Code: "+ String(responseCode));
      logMessage("REST >\tResponse Payload: "+ responsePayload);
      if(responseCode == HTTP_RESPONSE_OK || responseCode == HTTP_RESPONSE_CREATED){
        logMessage("REST >\tData sended");
        server_available = true;
      } else if(responseCode == HTTP_RESPONSE_CONFLICT){
        logMessage("REST >\tSome of the payload send exists in the server");
      } else if(responseCode == HTTP_RESPONSE_PARTIAL_CONTENT){
        logMessage("REST >\tSome of the payload sent is not valid");
      } else {
        logMessage("REST >\tCode: " + String(responseCode));
      }
    } else {
      logMessage("REST >\tNot connected to WiFi.");
      connectToWiFi();
    }
    //un código de respuesta 0 significará que el servidor no está disponible
    if(responseCode == 0){
      server_available = false;
    }
  } else {
    logMessage("REST >\tServer is not available");
  }

  return responseCode;
}
