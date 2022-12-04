#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#define LED D0
#define GER D2
#define RELAY D1

const char* ssid = "TP-Link_CC35";
const char* password = "66433187";
const String api_key = "poberezhnyi";
String serverName = "http://192.168.0.104:8000/";

const long utcOffsetInSeconds = 0;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);
HTTPClient http;

WiFiServer server(80);

// Variable to store the HTTP request
String header;

bool prev_gerkon_read = LOW;

// Current time
unsigned long currentTime = millis();
// Previous time
unsigned long previousTime = 0;
// Define timeout time in milliseconds (example: 2000ms = 2s)
const long timeoutTime = 2000;

void setup() {
  Serial.begin(115200);
  // Initialize the output variables as outputs
  pinMode(LED, OUTPUT);
  pinMode(GER, INPUT);
  pinMode(RELAY, OUTPUT);
  prev_gerkon_read = digitalRead(GER);
  // Set outputs to LOW
  digitalWrite(LED, LOW);
  digitalWrite(RELAY, HIGH);
  // Connect to Wi-Fi network with SSID and password
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED, LOW);
    delay(250);
    digitalWrite(LED, HIGH);
    delay(250);
    Serial.print(".");
  }
  // Print local IP address and start web server
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  server.begin();
  rebootLog();
}

void loop() {
  httpServer();
  checkGerkon();
  delay(100);
}

void checkGerkon(){
  bool gerkon_read = digitalRead(GER);
  if (gerkon_read != prev_gerkon_read) {
    if (gerkon_read == HIGH) {
      device_log("{"+apiKeyAndDateTime() + ",\"state\":\"1\"}", "api/gerkon/");
      digitalWrite(LED, HIGH);
    } else {
      device_log("{"+apiKeyAndDateTime() + ",\"state\":\"0\"}", "api/gerkon/");
      digitalWrite(LED, LOW);
    }
    prev_gerkon_read = gerkon_read;
  }
}

void rebootLog(){
  if((ESP.getResetReason()== "External System") || (ESP.getResetReason()== "Software/System restart")){
    device_log("{"+apiKeyAndDateTime()+",\"action\":\"Device rebooted, reason: "+ESP.getResetReason()+"\",\"type\":\"INFO\"}",  "api/device/");
  }
  else{
   device_log("{"+apiKeyAndDateTime()+",\"action\":\"Device rebooted, reason: "+ESP.getResetReason()+"\",\"type\":\"ERROR\"}",  "api/device/");
  }
  
}

void httpServer(){
  WiFiClient client = server.available();  // Listen for incoming clients
  if (client) {                            // If a new client connects,
    String currentLine = "";               // make a String to hold incoming data from the client
    currentTime = millis();
    previousTime = currentTime;
    while (client.connected() && currentTime - previousTime <= timeoutTime) {  // loop while the client's connected
      currentTime = millis();
      if (client.available()) {  // if there's bytes to read from the client,
        char c = client.read();  // read a byte, then
        header += c;
        if (c == '\n') {  // if the byte is a newline character
          if (currentLine.length() == 0) {
            client.println("HTTP/1.1 204 No Content");
            client.println("Connection: close");
            client.println();

            // turns the GPIOs on and off
            if (header.indexOf("GET /0/off") >= 0) {
              device_log("{"+apiKeyAndDateTime()+",\"action\":\"LED output was set LOW\",\"type\":\"INFO\"}",  "api/device/");
              digitalWrite(LED, LOW);
            } else if (header.indexOf("GET /0/on") >= 0) {
              device_log("{"+apiKeyAndDateTime()+",\"action\":\"LED output was set HIGH\",\"type\":\"INFO\"}",  "api/device/");
              digitalWrite(LED, HIGH);
            } else if (header.indexOf("GET /1/on") >= 0) {
              device_log("{"+apiKeyAndDateTime()+",\"action\":\"RELAY output was set LOW\",\"type\":\"INFO\"}",  "api/device/");
              digitalWrite(RELAY, LOW);
            } else if (header.indexOf("GET /1/off") >= 0) {
              device_log("{"+apiKeyAndDateTime()+",\"action\":\"RELAY output was set HIGH\",\"type\":\"INFO\"}",  "api/device/");
              digitalWrite(RELAY, HIGH);
            }
            else if (header.indexOf("GET /reset") >= 0) {
              device_log("{"+apiKeyAndDateTime()+",\"action\":\"Device was reset\",\"type\":\"INFO\"}",  "api/device/");
              ESP.reset();
            }
            break;
          } else {  // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    header = "";
    client.stop();
  }
}

void device_log(String msg, String p){
  WiFiClient client2;
  String serverPath = serverName + p;
  http.begin(client2, serverPath.c_str());
  http.addHeader("Content-Type", "application/json");
  int httpResponseCode = http.POST(msg);
  http.end();
}

String apiKeyAndDateTime(){
  timeClient.begin();
  timeClient.update();
  //Get a date structure
  time_t epochTime = timeClient.getEpochTime();
  struct tm *ptm = gmtime ((time_t *)&epochTime); 
  int monthDay = ptm->tm_mday;
  int currentMonth = ptm->tm_mon+1;
  int currentYear = ptm->tm_year+1900;
  String currentDate = String(currentYear) + "-" + String(currentMonth) + "-" + String(monthDay);
  String resultStr = "\"api_key\":\""+api_key+"\",\"date\":\""+currentDate+"\",\"time\":\""+timeClient.getFormattedTime()+"\"";
  timeClient.end();
  return resultStr;
}
