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

WiFiServer server(80);

// Variable to store the HTTP request
String header;

// Auxiliar variables to store the current output state
String output0State = "off";
String output1State = "off";
int prev_gerkon_read = LOW;
// Assign output variables to GPIO pins

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
  digitalWrite(RELAY, LOW);
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
}

void loop() {
  WiFiClient client = server.available();  // Listen for incoming clients
  if (client) {                            // If a new client connects,
    Serial.println("New Client.");         // print a message out in the serial port
    String currentLine = "";               // make a String to hold incoming data from the client
    currentTime = millis();
    previousTime = currentTime;
    while (client.connected() && currentTime - previousTime <= timeoutTime) {  // loop while the client's connected
      currentTime = millis();
      if (client.available()) {  // if there's bytes to read from the client,
        char c = client.read();  // read a byte, then
        Serial.write(c);         // print it out the serial monitor
        header += c;
        if (c == '\n') {  // if the byte is a newline character
          if (currentLine.length() == 0) {
            client.println("HTTP/1.1 204 No Content");
            client.println("Connection: close");
            client.println();

            // turns the GPIOs on and off
            if (header.indexOf("GET /0/off") >= 0) {
              output0State = "off";
              digitalWrite(LED, LOW);
            } else if (header.indexOf("GET /0/on") >= 0) {
              output0State = "on";
              digitalWrite(LED, HIGH);
            } else if (header.indexOf("GET /2") >= 0) {
              if (digitalRead(GER) == HIGH) {
                output1State = "on";
                digitalWrite(RELAY, HIGH);
              } else {
                output1State = "off";
                digitalWrite(RELAY, LOW);
              }
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
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
  int gerkon_read = digitalRead(GER);
  int httpResponseCode = -1;
  if (gerkon_read != prev_gerkon_read) {
    timeClient.begin();
    timeClient.update();
    //Get a date structure
    time_t epochTime = timeClient.getEpochTime();
    struct tm *ptm = gmtime ((time_t *)&epochTime); 
    int monthDay = ptm->tm_mday;
    int currentMonth = ptm->tm_mon+1;
    int currentYear = ptm->tm_year+1900;
    //Complete date:
    String currentDate = String(currentYear) + "-" + String(currentMonth) + "-" + String(monthDay);
    String postJSONdata = "{\"api_key\":\""+api_key+"\",\"date\":\""+currentDate+"\",\"time\":\""+timeClient.getFormattedTime()+"\"";
    timeClient.end();
    HTTPClient http;
    String serverPath = serverName + "api/gerkon/";
    if (gerkon_read == HIGH) {
      postJSONdata = postJSONdata + ",\"state\":\"1\"}";
      digitalWrite(LED, HIGH);
    } else {
      postJSONdata = postJSONdata + ",\"state\":\"0\"}";
      digitalWrite(LED, LOW);
    }
    http.begin(client, serverPath.c_str());
    http.addHeader("Content-Type", "application/json");
    int httpResponseCode = http.POST(postJSONdata);
    http.end();
    if (httpResponseCode == -1) {
    }
    prev_gerkon_read = gerkon_read;
  }
  delay(100);
}
