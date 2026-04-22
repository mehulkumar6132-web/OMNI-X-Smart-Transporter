#include <WiFi.h>
#include <FirebaseESP32.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// -CONFIG (HIDE REAL VALUES) ==================---
#define WIFI_SSID "YOUR_WIFI_NAME"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#define FIREBASE_HOST "YOUR_FIREBASE_HOST"
#define FIREBASE_AUTH "YOUR_FIREBASE_SECRET"

const int TRIG = 5, ECHO = 18, SERVO_PIN = 19, BUZZER = 13;
const int IN1=25, IN2=26, IN3=27, IN4=14;

Servo myServo;
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
Adafruit_SSD1306 display(128, 64, &Wire, -1);

void beep(int ms) { digitalWrite(BUZZER, HIGH); delay(ms); digitalWrite(BUZZER, LOW); }

// --- ANIMATED SCANNING EYES (VIDEO STYLE) ---
void drawScanningEyes(String dir) {
  // 1. Open Eyes with Pupils moving
  display.clearDisplay();
  int pupilOffset = (dir == "LEFT") ? -5 : 5;
  
  display.fillRoundRect(35, 20, 22, 28, 8, WHITE); // Left Eye Socket
  display.fillRoundRect(73, 20, 22, 28, 8, WHITE); // Right Eye Socket
  
  display.fillCircle(46 + pupilOffset, 34, 5, BLACK); // Pupil Left
  display.fillCircle(84 + pupilOffset, 34, 5, BLACK); // Pupil Right
  
  display.setTextSize(1);
  display.setCursor(35, 52);
  display.print("SCANNING "); display.println(dir);
  display.display();
  delay(400);

  // 2. Blink (Aankhein band)
  display.clearDisplay();
  display.fillRect(35, 32, 22, 4, WHITE); 
  display.fillRect(73, 32, 22, 4, WHITE);
  display.display();
  delay(150);
}

void showWelcome() {
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(2); 
  display.setCursor(20, 10); 
  display.println("WELCOME");
  display.setCursor(25, 35);
  display.println("OMNI-X");
  display.display();
  delay(2000);
}

void updateOLED(String msg) {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 20);
  display.println(msg);
  display.display();
}

long getDistance() {
  digitalWrite(TRIG, LOW); delayMicroseconds(2);
  digitalWrite(TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG, LOW);
  long duration = pulseIn(ECHO, HIGH, 30000);
  if (duration == 0) return 100; 
  return duration * 0.034 / 2;
}

void setup() {
  Serial.begin(115200);
  pinMode(TRIG, OUTPUT); pinMode(ECHO, INPUT);
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT); 
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  
  myServo.attach(SERVO_PIN); 
  myServo.write(90);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) Serial.println("OLED Error");
  
  showWelcome(); 
  updateOLED("CONNECTING");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) { delay(500); }

  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&config, &auth);
  
  updateOLED("READY"); 
  beep(500);
}

void loop() {
  long distance = getDistance();
  
  if (Firebase.getString(fbdo, "/Command")) {
    String cmd = fbdo.stringData();

    // 1. BACK PRIORITY (Ignore Sensor)
    if (cmd == "B") {
      digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH); digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH); 
      updateOLED("BACK v"); 
      beep(50);
    } 

    // 2. TURN OVERRIDE (Forced Turns)
    else if (cmd == "L") {
      digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH); digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW); 
      updateOLED("<-LEFT"); beep(50);
      delay(600); // Wait for turn
      Firebase.setString(fbdo, "/Command", "S");
    } 
    else if (cmd == "R") {
      digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW); digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH); 
      updateOLED("RIGHT->"); beep(50);
      delay(600);
      Firebase.setString(fbdo, "/Command", "S");
    }

    // 3. OBSTACLE SCANNING (With Video Eyes)
    else if (distance > 0 && distance < 50 && cmd != "S") { 
      digitalWrite(IN1, LOW); digitalWrite(IN2, LOW); digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
      beep(100); 

      myServo.write(0); 
      drawScanningEyes("RIGHT"); // Look Right Animation
      long dR = getDistance();
      
      myServo.write(180); 
      drawScanningEyes("LEFT"); // Look Left Animation
      long dL = getDistance();
      
      myServo.write(90); 
      delay(300);

      if (dR > dL) Firebase.setString(fbdo, "/PathInfo", "RIGHT CLEAR");
      else Firebase.setString(fbdo, "/PathInfo", "LEFT CLEAR");
      
      Firebase.setString(fbdo, "/Command", "S"); 
      updateOLED("STOP"); 
    }

    // 4. FORWARD & STOP
    else {
      if (cmd == "F" && distance > 50) { 
        digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW); digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW); 
        updateOLED("FORWARD ^");
      } else if (cmd == "S") {
        digitalWrite(IN1, LOW); digitalWrite(IN2, LOW); digitalWrite(IN3, LOW); digitalWrite(IN4, LOW); 
        updateOLED("STOP");
      }
    }
  }

  // HORN
  if (Firebase.getInt(fbdo, "/Buzzer")) {
    if (fbdo.intData() == 1) digitalWrite(BUZZER, HIGH);
    else digitalWrite(BUZZER, LOW);
  }
}

