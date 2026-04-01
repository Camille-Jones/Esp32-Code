/*
 * Simplified ESP32 LoRa Communication using REYAX RYLR998
 * This ESP32 acts as Node 1 with address 100
 * Sends a message to ESP8266 (address 200) ONLY when the button is pressed.
 * Receives messages and blinks LED.
 */

#include <HardwareSerial.h>
#include <WiFi.h>
#include <ctime>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>

// Pin definitions
#define BUTTON_PIN 4
#define LED_PIN 2
#define RX2_PIN 16
#define TX2_PIN 17

// LoRa configuration
#define LORA_ADDRESS 67       // This ESP32's address
#define DEST_ADDRESS 67        // arduino's address
#define NETWORK_ID 69            // Same for both devices
#define FREQUENCY_BAND 915000000 // 865 MHz (adjust based on your region)

// Create Serial2 instance for LoRa communication
HardwareSerial LoRaSerial(2);

//info for network created by esp32
const char* ssid = "ele";
const char* password = "123456789";

unsigned long lastSSE    = 0;
const unsigned long SSE_INTERVAL  = 500;

AsyncWebServer   server(80);
AsyncEventSource events("/events");

float  Vbatt2       = 0.0;
float  Vbatt1       = 0.0;
float  VShuntDC     = 0.0;
bool   MotorEnabled = false;
bool   BrakeEnabled = false;
bool   DMSEnabled   = false;
float  PowerPct     = 0.0;
float  Watts        = 0.0;

String buildJSON() {
  String j = "{";
  j += "\"Vbatt1\":"       + String(Vbatt1,   2) + ",";
  j += "\"Vbatt2\":"       + String(Vbatt2,   2) + ",";
  j += "\"VShuntDC\":"     + String(VShuntDC, 4) + ",";
  j += "\"MotorEnabled\":" + String(MotorEnabled ? "true" : "false") + ",";
  j += "\"BrakeEnabled\":" + String(BrakeEnabled ? "true" : "false") + ",";
  j += "\"DMSEnabled\":"   + String(DMSEnabled   ? "true" : "false") + ",";
  j += "\"PowerPct\":"     + String(PowerPct, 1) + ",";
  j += "\"Watts\":"        + String(Watts,    1);
  j += "}";
  return j;
}

String output26State = "off";
String output27State = "off";

unsigned long currentTime = millis();
// Previous time
unsigned long previousTime = 0; 
// Define timeout time in milliseconds (example: 2000ms = 2s)
const long timeoutTime = 2000;



String header;

// Set web server port number to 80
WiFiServer wifiserver(80);

void setup() {
  // Initialize Serial for debugging
  Serial.begin(115200);
  delay(1000);
  Serial.println("ESP32 LoRa Transceiver Starting...");

  //starts LittleFS
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS mount failed"); return;
  }
  Serial.println("LittleFS mounted OK");

  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nIP: " + WiFi.localIP().toString());

  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

  events.onConnect([](AsyncEventSourceClient* client) {
    client->send(buildJSON().c_str(), "data", millis(), 1000);
  });
  server.addHandler(&events);

  server.begin();

  // Initialize LoRa Serial
  LoRaSerial.begin(115200, SERIAL_8N1, RX2_PIN, TX2_PIN);

  // Setup pins
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Configure LoRa module
  setupLoRa();

  // Set up Wifi
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  wifiserver.begin();

  Serial.println("ESP32 Ready! Press button to send message.");
  Serial.println("Waiting for incoming messages...");
}

void setupLoRa() {
  delay(1000);

  // Test AT command
  sendATCommand("AT");

  // Set module address
  sendATCommand("AT+ADDRESS=" + String(LORA_ADDRESS));

  // Set network ID
  sendATCommand("AT+NETWORKID=" + String(NETWORK_ID));

  // Set frequency band
  sendATCommand("AT+BAND=" + String(FREQUENCY_BAND));

  // Set output power (optional, max is 22 dBm)
  sendATCommand("AT+CRFOP=22");

  // Get module parameters for verification
  sendATCommand("AT+PARAMETER?");
}

void sendATCommand(String command) {
  // Clear any pending data
  while (LoRaSerial.available()) {
    LoRaSerial.read();
  }
  
  LoRaSerial.println(command);
  delay(100);

  String response = "";
  unsigned long startTime = millis();

  while (millis() - startTime < 2000) {
    if (LoRaSerial.available()) {
      response += LoRaSerial.readString();
    }
  }

  if (response.length() > 0) {
    Serial.print("Command: ");
    Serial.print(command);
    Serial.print(" -> Response: ");
    Serial.println(response);
  }
}

String receiveMessage() {
  static String buffer = "";
  String message = "hi";

  while (LoRaSerial.available()) {
    char c = LoRaSerial.read();
    buffer += c;

    // Check for end of line
    if (c == '\n') {
      // Process the complete message
      if (buffer.indexOf("+RCV") != -1) {
        Serial.println("\n*** Received Message ***");
        
        // Parse the received message
        // Format: +RCV=address,length,data,RSSI,SNR
        int firstComma = buffer.indexOf(',');
        int secondComma = buffer.indexOf(',', firstComma + 1);
        int thirdComma = buffer.indexOf(',', secondComma + 1);
        int fourthComma = buffer.indexOf(',', thirdComma + 1);

        if (firstComma != -1 && secondComma != -1 && thirdComma != -1) {
          String senderAddress = buffer.substring(5, firstComma);
          String message = buffer.substring(secondComma + 1, thirdComma);
          
          Serial.print("From: ");
          Serial.println(senderAddress);
          Serial.print("Message: ");
          Serial.println(message);
          return message;

          if (fourthComma != -1) {
            String rssi = buffer.substring(thirdComma + 1, fourthComma);
            String snr = buffer.substring(fourthComma + 1);
            Serial.print("RSSI: ");
            Serial.print(rssi);
            Serial.print(" dBm, SNR: ");
            Serial.println(snr);
          }
          
          Serial.println("************************\n");
          
          // Blink LED when message received
          blinkLED(3, 200);

        }
      }
      // Clear buffer after processing
      buffer = "";
    }
  }
  return message;
}

void receiveMessageVoid() {
  static String buffer = "";

  while (LoRaSerial.available()) {
    char c = LoRaSerial.read();
    buffer += c;

    // Check for end of line
    if (c == '\n') {
      // Process the complete message
      if (buffer.indexOf("+RCV") != -1) {
        Serial.println("\n*** Received Message ***");
        
        // Parse the received message
        // Format: +RCV=address,length,data,RSSI,SNR
        int firstComma = buffer.indexOf(',');
        int secondComma = buffer.indexOf(',', firstComma + 1);
        int thirdComma = buffer.indexOf(',', secondComma + 1);
        int fourthComma = buffer.indexOf(',', thirdComma + 1);

        if (firstComma != -1 && secondComma != -1 && thirdComma != -1) {
          String senderAddress = buffer.substring(5, firstComma);
          String message = buffer.substring(secondComma + 1, thirdComma);
          
          Serial.print("From: ");
          Serial.println(senderAddress);
          Serial.print("Message: ");
          Serial.println(message);
          

          if (fourthComma != -1) {
            String rssi = buffer.substring(thirdComma + 1, fourthComma);
            String snr = buffer.substring(fourthComma + 1);
            Serial.print("RSSI: ");
            Serial.print(rssi);
            Serial.print(" dBm, SNR: ");
            Serial.println(snr);
          }
          
          Serial.println("************************\n");
          
          // Blink LED when message received
          blinkLED(3, 200);
        }
      }
      // Clear buffer after processing
      buffer = "";
    }
  }
}

void blinkLED(int times, int delayMs) {
  for (int i = 0; i < times; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(delayMs);
    digitalWrite(LED_PIN, LOW);
    delay(delayMs);
  }
}

void loop() {

  unsigned long now = millis();
  if (now - lastSSE >= SSE_INTERVAL) {
    lastSSE = now;
    events.send(buildJSON().c_str(), "data", now);
  }
  
  srand(time(0));

  int randomNumber = rand() % 90 + 10; 

  WiFiClient client = wifiserver.available();   // Listen for incoming clients

  //Serial.println(receiveMessage());
  receiveMessageVoid();

  if (client) {                             // If a new client connects,
    currentTime = millis();
    previousTime = currentTime;
    Serial.println("New Client.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected() && currentTime - previousTime <= timeoutTime) {  // loop while the client's connected
      currentTime = millis();
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();
            
            
            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            // CSS to style the on/off buttons 
            // Feel free to change the background-color and font-size attributes to fit your preferences
            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println(".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px;");
            client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
            client.println(".button2 {background-color: #555555;}</style></head>");
            
            // Web Page Heading
            client.println("<body><h1>ESP32 Web Server</h1>");
            
            // Display current message
            client.println("<p>Message " + String(randomNumber) + "</p>");
               
            // The HTTP response ends with another blank line
            client.println();
            // Break out of the while loop
            break;
          } else { // if you got a newline, then clear currentLine
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
}
