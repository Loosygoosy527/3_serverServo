/*
  DM3014 Interactive Devices, Ashley Hi 2026
  Week 9 - Remote Interfaces
  ESP32-C3 Server Servo Control
  Move a 180° servo motor over web server.
  Additional Components: Servo Motor
*/

// ====== Reference Libraries ======
#include <WiFi.h>
#include <ESP32Servo.h>

// ====== Network Credentials ======
const char* ssid     = "MyLEDBoard"; // *** edit AP name here
const char* password = "week9class"; // *** edit password here

// ====== Network Settings ======
WiFiServer server(80);
String header;        // variable stores HTTP request
int servo_angle = 90; // variable stores current servo angle (0-180)

// ====== Wave Settings ======
const int WAVE_MIN = 22;   // 22 to 157 = 135 degrees of movement
const int WAVE_MAX = 157;
const int WAVE_DELAY = 10; // ms between each step (smaller = faster wave)
bool waving = false;       // is the servo currently waving?
int wave_dir = 1;          // 1 = moving up, -1 = moving down
unsigned long lastStep = 0;

// ====== Assign Servo Pin ======
#define SERVO_PIN 7
Servo myServo;

// ====== Webpage ======
// *** edit the page here, written exactly like a normal HTML file
// %ANGLE% and %STATUS% are placeholders: they get swapped for the current values
const char webpage[] = R"HTML(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="icon" href="data:,">
  <style>
    html { font-family: Helvetica; text-align: center; }
    body { background-color: #ADD8E6; color: #222; }
    h1 { color: black; }

    /* one row across the screen, never wraps */
    .row {
      display: flex;
      flex-wrap: nowrap;
      justify-content: center;
      gap: 2vw;
      margin: 20px 0;
    }
    .row a { text-decoration: none; }

    .button {
      color: white;
      border: none;
      border-radius: 50%;
      width: min(16vw, 110px);
      height: min(16vw, 110px);
      padding: 0;
      font-size: min(4.5vw, 28px);
      cursor: pointer;
    }
    .big {
      width: min(26vw, 150px);
      height: min(26vw, 150px);
      font-size: min(6vw, 34px);
    }

    /* a different colour for every circle */
    .c1 { background-color: #F44336; } /* red */
    .c2 { background-color: #FF9800; } /* orange */
    .c3 { background-color: #4CAF50; } /* green */
    .c4 { background-color: #2196F3; } /* blue */
    .c5 { background-color: #9C27B0; } /* purple */
    .wave { background-color: #E91E63; } /* pink */
    .stop { background-color: #607D8B; } /* grey */
  </style>
</head>
<body>
  <h1>My Servo Controller</h1>
  <p>Servo Angle - %ANGLE%&deg;</p>
  <p>%STATUS%</p>

  <!-- add more angles by copying a line and changing both numbers -->
  <div class="row">
    <a href="/servo/0"><button class="button c1">-90&deg;</button></a>
    <a href="/servo/45"><button class="button c2">-45&deg;</button></a>
    <a href="/servo/90"><button class="button c3">0&deg;</button></a>
    <a href="/servo/135"><button class="button c4">+45&deg;</button></a>
    <a href="/servo/180"><button class="button c5">+90&deg;</button></a>
  </div>

  <div class="row">
    <a href="/wave"><button class="button big wave">WAVE</button></a>
    <a href="/stop"><button class="button big stop">STOP</button></a>
  </div>
</body>
</html>
)HTML";

void setup() {
  // ====== Setup Serial Monitor ======
  Serial.begin(115200);

  // ====== Setup Servo ======
  myServo.setPeriodHertz(50);           // standard servos expect 50 Hz
  myServo.attach(SERVO_PIN, 500, 2400); // pulse width range in microseconds
  myServo.write(servo_angle);           // move to start position

  // ====== Setup ESP as Access Point ======
  Serial.print("Setting AP (Access Point)…");
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("IP Address: ");
  Serial.println(IP);
  server.begin();
}

void loop(){
  WiFiClient client = server.available(); // listen for incoming clients
  if (client) {                           // if a new client connects
    Serial.println("New Client");
    String currentLine = "";
    unsigned long startTime = millis();   // note when client connected (for timeout)
    while (client.connected() && millis() - startTime < 2000) { // set a 2s timeout
      if (client.available()) {           // if there's bytes to read from the client
        char c = client.read();           // read a byte
        Serial.write(c);
        header += c;
        if (c == '\n') {
          if (currentLine.length() == 0) {
            // ====== Setup Webpage ======
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();

            // ====== Start / Stop Wave ======
            if (header.indexOf("GET /wave") >= 0) {
              waving = true;
              servo_angle = constrain(servo_angle, WAVE_MIN, WAVE_MAX);
              Serial.println("wave started");
            } else if (header.indexOf("GET /stop") >= 0) {
              waving = false;
              Serial.println("wave stopped");
            }

            // ====== Move Servo ======
            int pos = header.indexOf("GET /servo/");
            if (pos >= 0) {
              waving = false; // pressing an angle button stops the wave
              servo_angle = constrain(header.substring(pos + 11).toInt(), 0, 180); // read the number after /servo/
              Serial.println("servo to " + String(servo_angle));
              myServo.write(servo_angle);
            }

            // ====== Display HTML Webpage ======
            String page = webpage;                                // copy webpage from top of the sketch
            page.replace("%ANGLE%", String(servo_angle - 90));    // show angle with 90 as zero
            page.replace("%STATUS%", waving ? "Waving..." : "");  // show wave status
            client.print(page);                                   // send it to the browser
            client.println();                                     // HTTP response ends with another blank line
            break;                                                // break out of the while loop
          } else {
            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c;
        }
      }
    }
    header = "";   // clear the header variable
    client.stop(); // close the connection
    Serial.println("Client disconnected.");
    Serial.println("");
  }

  // ====== Wave Motion (runs in the background) ======
  if (waving && millis() - lastStep >= WAVE_DELAY) {
    lastStep = millis();
    servo_angle += wave_dir;
    if (servo_angle >= WAVE_MAX) { servo_angle = WAVE_MAX; wave_dir = -1; }
    if (servo_angle <= WAVE_MIN) { servo_angle = WAVE_MIN; wave_dir = 1; }
    myServo.write(servo_angle);
  }
}