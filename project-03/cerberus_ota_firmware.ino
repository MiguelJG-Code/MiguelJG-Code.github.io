/*
 * CERBERUS MULTI-MODE BATTLE BOT — OTA + Telemetry Edition
 * Sumo | Hockey (BLE) | Maze Solver | Line Follower
 * Board : ESP32 WROOM 38-pin
 * OTA   : http://<ESP32-IP>/update
 * BLE   : device name "HC05" — F/B/L/R/S commands
 * WiFi  : active for Sumo / Maze / Line — OFF for Hockey
 */

#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include "BluetoothSerial.h"

// WiFi
const char* SSID = "FullyPaid";
const char* PASS = "12345678";

// DIP Switches
#define SW1_SUMO    15
#define SW2_HOCKEY  16
#define SW3_MAZE    12
#define SW4_LINE     4

// Motor Driver L298N
#define ENA  32
#define IN1  25
#define IN2  26
#define ENB  33
#define IN3  27
#define IN4  14

// Ultrasonic Sensors
#define TRIG_FRONT   5
#define ECHO_FRONT  18
#define TRIG_LEFT   19
#define ECHO_LEFT   21
#define TRIG_RIGHT  23
#define ECHO_RIGHT  22

// IR Sensors (bottom)
#define IR_LEFT   34
#define IR_RIGHT  35

// Speed
#define SPEED_FULL    255
#define SPEED_TURN    180
#define SPEED_SLOW    150
#define SPEED_SEARCH  200

// Sumo distance thresholds (cm)
#define DETECT_DIST  20
#define WALL_DIST    15

// Telemetry
#define SENSOR_INTERVAL_MS  300
#define MAX_TCP_CLIENTS       4
#define US_TIMEOUT_US     58000UL
#define US_SETTLE_MS         30
#define MODE_ANNOUNCE_MS   5000

// Maze constants (tune MAZE_TURN_90_MS to your bot)
#define MAZE_WALL_CM     30
#define MAZE_OPEN_CM     40
#define MAZE_TURN_90_MS  350
#define MAZE_TURN_180_MS 700
#define MAZE_FWD_MS       80
#define MAZE_NUDGE_MS    150

BluetoothSerial SerialBT;

WebServer  webServer(80);
WiFiServer tcpServer(81);
WiFiClient tcpClients[MAX_TCP_CLIENTS];

unsigned long lastSensorTime   = 0;
unsigned long lastModeAnnounce = 0;

bool wifiActive = false;
bool bleActive  = false;

const char* currentMode = "NONE";
const char* lastMode    = "NONE";
volatile char bleCmd    = 'S';

// Ultrasonic 

long readOneSensor(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(4);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long dur = pulseIn(echoPin, HIGH, US_TIMEOUT_US);
  if (dur == 0) return 9999;
  long cm = dur / 58;
  if (cm < 2 || cm > 400) return 9999;
  return cm;
}

long getDistance(int trigPin, int echoPin) {
  long r[3];
  for (int i = 0; i < 3; i++) {
    r[i] = readOneSensor(trigPin, echoPin);
    delayMicroseconds(5000);
  }
  if (r[0] > r[1]) { long t = r[0]; r[0] = r[1]; r[1] = t; }
  if (r[1] > r[2]) { long t = r[1]; r[1] = r[2]; r[2] = t; }
  if (r[0] > r[1]) { long t = r[0]; r[0] = r[1]; r[1] = t; }
  return r[1];
}

void readAllSensors(long &us1, long &us2, long &us3) {
  us1 = readOneSensor(TRIG_FRONT, ECHO_FRONT); delay(US_SETTLE_MS);
  us2 = readOneSensor(TRIG_LEFT,  ECHO_LEFT);  delay(US_SETTLE_MS);
  us3 = readOneSensor(TRIG_RIGHT, ECHO_RIGHT); delay(US_SETTLE_MS);
}

//  WiFi 

void startWifi() {
  if (wifiActive) return;
  Serial.println("[WIFI] Starting...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASS);
  Serial.printf("[WIFI] Connecting to %s", SSID);
  unsigned long t = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t < 10000) {
    delay(500); Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[WIFI] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
    webServer.on("/", HTTP_GET, []() {
      webServer.send(200, "text/plain", "Cerberus OTA+Telemetry OK");
    });
    webServer.on("/update", HTTP_GET, []() {
      webServer.send(200, "text/html",
        "<html><head><title>OTA</title></head>"
        "<body style='background:#0d0d0d;color:#e8e8e8;font-family:monospace;padding:20px'>"
        "<h2 style='color:#00e5ff'>Cerberus OTA Update</h2>"
        "<form method='POST' action='/update' enctype='multipart/form-data'>"
        "<input type='file' name='update' accept='.bin'><br><br>"
        "<input type='submit' value='Flash Firmware' "
        "style='padding:10px 20px;background:#00e5ff;color:#0d0d0d;border:none;cursor:pointer'>"
        "</form></body></html>");
    });
    webServer.on("/update", HTTP_POST,
      []() {
        webServer.send(200, "text/plain", Update.hasError() ? "FAIL" : "OK! Rebooting...");
        delay(500); ESP.restart();
      },
      []() {
        HTTPUpload &upload = webServer.upload();
        if      (upload.status == UPLOAD_FILE_START)  Update.begin(UPDATE_SIZE_UNKNOWN);
        else if (upload.status == UPLOAD_FILE_WRITE)  Update.write(upload.buf, upload.currentSize);
        else if (upload.status == UPLOAD_FILE_END)
          if (Update.end(true)) Serial.printf("\n[OTA] Done: %u bytes\n", upload.totalSize);
          else Update.printError(Serial);
      }
    );
    webServer.begin();
    Serial.println("[HTTP] Web server started on port 80");
    tcpServer.begin();
    tcpServer.setNoDelay(true);
    Serial.println("[TCP] Telemetry server started on port 81");
    wifiActive = true;
  } else {
    Serial.println("\n[WIFI] Failed to connect!");
  }
}

void stopWifi() {
  if (!wifiActive) return;
  for (int i = 0; i < MAX_TCP_CLIENTS; i++)
    if (tcpClients[i]) tcpClients[i].stop();
  tcpServer.end();
  webServer.close();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  wifiActive = false;
  Serial.println("[WIFI] Disabled.");
}

// Bluetooth 

void startBle() {
  if (bleActive) return;
  SerialBT.begin("HC05");
  bleActive = true;
  bleCmd = 'S';
  Serial.println("[BT] Started as 'HC05'");
}

void stopBle() {
  if (!bleActive) return;
  SerialBT.flush();
  SerialBT.end();
  bleActive = false;
  bleCmd = 'S';
  stopMotors();
  Serial.println("[BT] Stopped.");
}

// Telemetry

void broadcastTelemetry() {
  if (!wifiActive) return;
  if (tcpServer.hasClient()) {
    WiFiClient incoming = tcpServer.accept();
    bool slotFound = false;
    for (int i = 0; i < MAX_TCP_CLIENTS; i++) {
      if (!tcpClients[i] || !tcpClients[i].connected()) {
        tcpClients[i] = incoming;
        slotFound = true; break;
      }
    }
    if (!slotFound) { incoming.println("{\"error\":\"max clients reached\"}"); incoming.stop(); }
  }
  unsigned long now = millis();
  if (now - lastSensorTime >= SENSOR_INTERVAL_MS) {
    lastSensorTime = now;
    long us1, us2, us3;
    readAllSensors(us1, us2, us3);
    int ir1 = digitalRead(IR_LEFT);
    int ir2 = digitalRead(IR_RIGHT);
    char json[120];
    snprintf(json, sizeof(json),
      "{\"US1\":%ld,\"US2\":%ld,\"US3\":%ld,\"IR1\":%d,\"IR2\":%d,\"MODE\":\"%s\"}",
      us1, us2, us3, ir1, ir2, currentMode);
    Serial.println(json);
    for (int i = 0; i < MAX_TCP_CLIENTS; i++) {
      if (tcpClients[i] && tcpClients[i].connected()) tcpClients[i].println(json);
      else if (tcpClients[i]) tcpClients[i].stop();
    }
  }
}

void broadcastMode() {
  if (!wifiActive) return;
  unsigned long now = millis();
  if (now - lastModeAnnounce < MODE_ANNOUNCE_MS) return;
  lastModeAnnounce = now;
  char json[48];
  snprintf(json, sizeof(json), "{\"MODE\":\"%s\"}", currentMode);
  Serial.println(json);
  for (int i = 0; i < MAX_TCP_CLIENTS; i++) {
    if (tcpClients[i] && tcpClients[i].connected()) tcpClients[i].println(json);
    else if (tcpClients[i]) tcpClients[i].stop();
  }
}

// Setup

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  Serial.begin(115200);
  delay(500);
  Serial.println("\n[BOOT] Cerberus starting...");

  pinMode(SW1_SUMO,   INPUT_PULLUP);
  pinMode(SW2_HOCKEY, INPUT_PULLUP);
  pinMode(SW3_MAZE,   INPUT_PULLUP);
  pinMode(SW4_LINE,   INPUT_PULLUP);

  pinMode(ENA, OUTPUT); pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(ENB, OUTPUT); pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);

  pinMode(TRIG_FRONT, OUTPUT); pinMode(ECHO_FRONT, INPUT);
  pinMode(TRIG_LEFT,  OUTPUT); pinMode(ECHO_LEFT,  INPUT);
  pinMode(TRIG_RIGHT, OUTPUT); pinMode(ECHO_RIGHT, INPUT);
  digitalWrite(TRIG_FRONT, LOW);
  digitalWrite(TRIG_LEFT,  LOW);
  digitalWrite(TRIG_RIGHT, LOW);

  pinMode(IR_LEFT,  INPUT);
  pinMode(IR_RIGHT, INPUT);

  stopMotors();
  Serial.println("Ready! SW1=Sumo | SW2=Hockey | SW3=Maze | SW4=Line");
  // Start BT immediately so HC05 appears in phone scan right away
  startBle();
  delay(2000);
}

// Loop 

void loop() {
  bool sw1 = (digitalRead(SW1_SUMO)   == LOW);
  bool sw2 = (digitalRead(SW2_HOCKEY) == LOW);
  bool sw3 = (digitalRead(SW3_MAZE)   == LOW);
  bool sw4 = (digitalRead(SW4_LINE)   == LOW);
  bool hockeyMode = sw2 || (sw3 && sw4);

  if (hockeyMode) {
    if (wifiActive) stopWifi();
    if (!bleActive) startBle();
  } else {
    if (!wifiActive) startWifi();
    if (wifiActive) webServer.handleClient();
  }

  if (hockeyMode) {
    currentMode = "HOCKEY";
    lastMode = currentMode;
    runHockey();
  } else if (sw1) {
    currentMode = "SUMO";
    if (currentMode != lastMode) {
      stopMotors();
      Serial.println("SUMO starting in 5s...");
      for (int i = 5; i > 0; i--) { Serial.printf("  %d...\n", i); delay(1000); }
      lastMode = currentMode;
    }
    runSumo();
    broadcastTelemetry();
    broadcastMode();
  } else if (sw3) {
    currentMode = "MAZE";
    if (currentMode != lastMode) {
      stopMotors();
      Serial.println("MAZE starting in 5s...");
      for (int i = 5; i > 0; i--) { Serial.printf("  %d...\n", i); delay(1000); }
      lastMode = currentMode;
    }
    runMaze();
    broadcastTelemetry();
    broadcastMode();
  } else if (sw4) {
    currentMode = "LINE";
    if (currentMode != lastMode) {
      stopMotors();
      Serial.println("LINE starting in 5s...");
      for (int i = 5; i > 0; i--) { Serial.printf("  %d...\n", i); delay(1000); }
      lastMode = currentMode;
    }
    runLineFollow();
    broadcastTelemetry();
    broadcastMode();
  } else {
    currentMode = "NONE";
    lastMode = currentMode;
    stopMotors();
    broadcastTelemetry();
    broadcastMode();
  }

  delay(50);
}

// Mode 1: Sumo 
// IR sensors are front-facing — LOW = enemy detected
// Ultrasonic backs up IR for range and side detection
// No edge IR — bot never retreats unless pushed to boundary

void runSumo() {
  bool irLeft  = (digitalRead(IR_LEFT)  == LOW);  // LOW = enemy in front-left
  bool irRight = (digitalRead(IR_RIGHT) == LOW);  // LOW = enemy in front-right

  // Both IR see enemy — charge straight full speed
  if (irLeft && irRight) {
    moveForward(SPEED_FULL);
    return;
  }

  // Only left IR sees enemy turn left to align then charge
  if (irLeft && !irRight) {
    turnLeft(SPEED_TURN); delay(80);
    moveForward(SPEED_FULL);
    return;
  }

  // Only right IR sees enemy  turn right to align then charge
  if (!irLeft && irRight) {
    turnRight(SPEED_TURN); delay(80);
    moveForward(SPEED_FULL);
    return;
  }

  // IR sees nothing use ultrasonic to find enemy
  long distFront = getDistance(TRIG_FRONT, ECHO_FRONT);
  long distLeft  = getDistance(TRIG_LEFT,  ECHO_LEFT);
  long distRight = getDistance(TRIG_RIGHT, ECHO_RIGHT);

  if (distFront != 9999 && distFront < DETECT_DIST) {
    moveForward(SPEED_FULL);
    return;
  }
  if (distLeft != 9999 && distLeft < DETECT_DIST) {
    turnLeft(SPEED_TURN); delay(150);
    moveForward(SPEED_FULL);
    return;
  }
  if (distRight != 9999 && distRight < DETECT_DIST) {
    turnRight(SPEED_TURN); delay(150);
    moveForward(SPEED_FULL);
    return;
  }

  // Nothing found — spin to search
  turnRight(SPEED_SEARCH);
  delay(100);
}

// Mode 2: Hockey
// BLE commands: F/B/L/R/S — WiFi OFF while active

void runHockey() {
  if (SerialBT.available()) {
    bleCmd = SerialBT.read();
    Serial.print("[BT] CMD: "); Serial.println(bleCmd);
  }
  switch (bleCmd) {
    case 'F': case 'f': moveForward(SPEED_FULL);  break;
    case 'B': case 'b': moveBackward(SPEED_FULL); break;
    case 'R': case 'r': turnLeft(SPEED_TURN);     break;
    case 'L': case 'l': turnRight(SPEED_TURN);    break;
    default:            stopMotors();              break;
  }
}

// Mode 3: Maze 
// Left-hand rule. 9999 = no echo = open space.
// frontWall  : valid echo < MAZE_WALL_CM
// leftOpen / rightOpen : 9999 OR reading > MAZE_OPEN_CM

void runMaze() {
  long distFront = getDistance(TRIG_FRONT, ECHO_FRONT);
  long distLeft  = getDistance(TRIG_LEFT,  ECHO_LEFT);
  long distRight = getDistance(TRIG_RIGHT, ECHO_RIGHT);

  bool frontWall = (distFront != 9999 && distFront < MAZE_WALL_CM);
  bool leftOpen  = (distLeft  == 9999 || distLeft  > MAZE_OPEN_CM);
  bool rightOpen = (distRight == 9999 || distRight > MAZE_OPEN_CM);

  Serial.printf("[MAZE] F:%ld(%s) L:%ld(%s) R:%ld(%s)\n",
    distFront, frontWall ? "WALL" : "ok",
    distLeft,  leftOpen  ? "OPEN" : "wall",
    distRight, rightOpen ? "OPEN" : "wall");

  // No wall ahead keep driving forward continuously.
  // Re-read front sensor in a tight loop so it reacts the moment a wall appears.
  if (!frontWall) {
    moveForward(SPEED_SLOW);
    while (true) {
      long f = getDistance(TRIG_FRONT, ECHO_FRONT);
      if (f != 9999 && f < MAZE_WALL_CM) break;  // wall detected — stop and turn
    }
    stopMotors();
    delay(60);
    // Re-read all sensors now that we've stopped at the wall
    distLeft  = getDistance(TRIG_LEFT,  ECHO_LEFT);
    distRight = getDistance(TRIG_RIGHT, ECHO_RIGHT);
    leftOpen  = (distLeft  == 9999 || distLeft  > MAZE_OPEN_CM);
    rightOpen = (distRight == 9999 || distRight > MAZE_OPEN_CM);
  }

  // Wall in front — decide which way to turn
  if (leftOpen) {
    turnLeft(SPEED_TURN); delay(MAZE_TURN_90_MS);
    stopMotors(); delay(50);
    moveForward(SPEED_SLOW); delay(MAZE_NUDGE_MS); stopMotors();
    return;
  }

  if (rightOpen) {
    turnRight(SPEED_TURN); delay(MAZE_TURN_90_MS);
    stopMotors(); delay(50);
    moveForward(SPEED_SLOW); delay(MAZE_NUDGE_MS); stopMotors();
    return;
  }

  // All blocked — U-turn
  turnRight(SPEED_TURN); delay(MAZE_TURN_180_MS);
  stopMotors(); delay(50);
  moveForward(SPEED_SLOW); delay(MAZE_NUDGE_MS); stopMotors();
}

// Mode 4: Line Follower 
// IR HIGH = black line, LOW = white paper
// pivotLeft/pivotRight: one motor only for smooth line hugging
// lastTurn: remembers which side the line was last seen on for recovery

int lastTurn = 0; // -1 = line was left, 1 = line was right, 0 = straight

void pivotLeft(int speed) {
  analogWrite(ENA, 0);     analogWrite(ENB, speed);
  digitalWrite(IN1, LOW);  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
}

void pivotRight(int speed) {
  analogWrite(ENA, speed); analogWrite(ENB, 0);
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);  digitalWrite(IN4, LOW);
}

void runLineFollow() {
  bool leftOnLine  = (digitalRead(IR_LEFT)  == HIGH);
  bool rightOnLine = (digitalRead(IR_RIGHT) == HIGH);

  if (leftOnLine && rightOnLine) {
    // Both on line — go straight
    lastTurn = 0;
    moveForward(SPEED_SLOW);
  } else if (leftOnLine && !rightOnLine) {
    // Line to the left — pivot left
    lastTurn = -1;
    pivotLeft(SPEED_SLOW);
  } else if (!leftOnLine && rightOnLine) {
    // Line to the right — pivot right
    lastTurn = 1;
    pivotRight(SPEED_SLOW);
  } else {
    // Lost the line — sweep back toward last known side slowly
    if (lastTurn <= 0) {
      pivotLeft(SPEED_SLOW);   // last seen left or straight → sweep left
    } else {
      pivotRight(SPEED_SLOW);  // last seen right → sweep right
    }
  }
}

// Motor Functions 
  analogWrite(ENA, speed); analogWrite(ENB, speed);
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
}
void moveBackward(int speed) {
  analogWrite(ENA, speed); analogWrite(ENB, speed);
  digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH);
}
void turnLeft(int speed) {
  analogWrite(ENA, speed); analogWrite(ENB, speed);
  digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
}
void turnRight(int speed) {
  analogWrite(ENA, speed); analogWrite(ENB, speed);
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH);
}
void stopMotors() {
  analogWrite(ENA, 0); analogWrite(ENB, 0);
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
}
