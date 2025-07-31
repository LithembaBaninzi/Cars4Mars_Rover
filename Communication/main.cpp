#include <WiFi.h>
#include <LittleFS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

AsyncWebServer server(80);

const char* ssid = "ROVER_WIFI";
const char* password = "pass123456";

// === Motor Control Setup (IBT-2 Driver) ===
// Using safe GPIOs (avoid 6, 7, 8, 9, 10, 11)
const int RPWM_Output = 26;   // ESP32 → IBT-2 RPWM (Forward)
const int LPWM_Output = 25;   // ESP32 → IBT-2 LPWM (Backward)

// === Encoder Setup ===
#define Encoder_output_A 12  // Encoder Channel A (safe interrupt pin)
#define Encoder_output_B 13  // Encoder Channel B (safe pin)
#define PULSES_PER_ROTATION 3 // 3 pulses = 1 full rotation

// Encoder variables
volatile long encoderValue = 0;
unsigned long previousMillis = 0;
const unsigned long interval = 1000; // 1-second interval for RPM calculation
int rpm = 0;

// Motor control variables
String currentDirection = "Stopped";
int currentSpeed = 50; // Speed as percentage (0-100)
int motor_speed = map(currentSpeed, 0, 100, 0, 255); // Mapped PWM value
unsigned long startMillis;

// LEDC PWM channels
#define PWM_FREQ 20000     // 20kHz for IBT-2
#define PWM_RESOLUTION 8   // 8-bit resolution (0-255)
#define PWM_CHANNEL_RPWM 0 // Channel for RPWM
#define PWM_CHANNEL_LPWM 1 // Channel for LPWM

// Function declarations
void robot_setup();
void robot_stop();
void robot_fwd();
void robot_back();
void robot_left();
void robot_right();
void setMotorSpeed(int pwmValue, String direction);
void IRAM_ATTR DC_Motor_Encoder();

void setup() {
  Serial.begin(9600);
  
  // Initialize motor control
  robot_setup();

  // Initialize encoder
  pinMode(Encoder_output_A, INPUT_PULLUP);
  pinMode(Encoder_output_B, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(Encoder_output_A), DC_Motor_Encoder, RISING);

  WiFi.softAP(ssid, password);
  Serial.println("ESP32 Access Point Started");
  Serial.println(WiFi.softAPIP());

  if (!LittleFS.begin()) {
    Serial.println("LittleFS Mount Failed");
    // LittleFS.format();
    // ESP.restart();
    //return;
  } else {
    Serial.println("LittleFS Mounted Successfully");
  }

  startMillis = millis();

  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
  // More robust version:
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/index.html", "text/html");
  });

  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/style.css", "text/css");
  });

  server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/script.js", "application/javascript");
  });
  // Movement commands
  server.on("/left", HTTP_GET, [](AsyncWebServerRequest *request){
    robot_left();
    currentDirection = "Left";
    request->send(200, "text/plain", "LEFT");
    Serial.println("LEFT button pressed");
  });

  server.on("/up", HTTP_GET, [](AsyncWebServerRequest *request){
    robot_fwd();
    currentDirection = "Forward";
    request->send(200, "text/plain", "UP");
    Serial.println("UP button pressed");
  });

  server.on("/right", HTTP_GET, [](AsyncWebServerRequest *request){
    robot_right();
    currentDirection = "Right";
    request->send(200, "text/plain", "RIGHT");
    Serial.println("RIGHT button pressed");
  });

  server.on("/down", HTTP_GET, [](AsyncWebServerRequest *request){
    robot_back();
    currentDirection = "Reverse";
    request->send(200, "text/plain", "DOWN");
    Serial.println("DOWN button pressed");
  });

  server.on("/stop", HTTP_GET, [](AsyncWebServerRequest *request){
    robot_stop();
    currentDirection = "Stopped";
    request->send(200, "text/plain", "STOP");
    Serial.println("STOP button pressed");
  });

  // Speed control handler
  server.on("/speed", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("value")) {
      currentSpeed = request->getParam("value")->value().toInt();
      motor_speed = map(currentSpeed, 0, 100, 0, 255);

    // Debug output - add this line:
    Serial.println("Speed changed to: "+String(currentSpeed)+", PWM value: " + String(motor_speed));

      // Apply speed to current direction
      if (currentDirection != "Stopped") {
        setMotorSpeed(motor_speed, currentDirection);
      }
      request->send(200, "text/plain", "Speed set to " + String(currentSpeed));
      Serial.println("Speed updated to: " + String(currentSpeed));
    } else {
      request->send(400, "text/plain", "Missing speed value");
    }
  });

  // System status report (now includes RPM)
  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request){
    int rssi = WiFi.RSSI();
    unsigned long uptimeSeconds = (millis() - startMillis) / 1000;
    
    String json = "{";
    json += "\"direction\":\"" + currentDirection + "\",";
    json += "\"speed\":" + String(currentSpeed) + ",";
    json += "\"rpm\":" + String(rpm) + ",";
    json += "\"encoder\":" + String(encoderValue) + ",";
    json += "\"signal\":" + String(rssi) + ",";
    json += "\"uptime\":" + String(uptimeSeconds);
    json += "}";
    request->send(200, "application/json", json);
  });


  // Restart handler
  server.on("/restart", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", "Restarting...");
    delay(100);
    ESP.restart();
  });

  // 404 handler for missing files
  server.onNotFound([](AsyncWebServerRequest *request){
    if(request->method() == HTTP_GET){
      // Try to serve the file, if not found return 404
      if(!LittleFS.exists(request->url())) {
        request->send(404, "text/plain", "File not found");
        return;
      }
    }
    request->send(404);
  });

  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(204);
  });
  
  server.begin();
}

void loop() {
  // RPM calculation and display
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    
    // Calculate RPM
    rpm = (encoderValue * 60) / PULSES_PER_ROTATION;
    
    Serial.print("Direction: " + currentDirection);
    Serial.print(" | Speed: " + String(currentSpeed) + "%");
    Serial.print(" | Encoder Pulses: ");
    Serial.print(encoderValue);
    Serial.print(" → RPM: ");
    Serial.println(rpm);
    
    encoderValue = 0; // Reset count
  }
}

// === Motor Control Functions ===
void robot_setup() {
  // Configure PWM channels
  ledcSetup(PWM_CHANNEL_RPWM, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(PWM_CHANNEL_LPWM, PWM_FREQ, PWM_RESOLUTION);
  
  // Attach pins to PWM channels
  ledcAttachPin(RPWM_Output, PWM_CHANNEL_RPWM);
  ledcAttachPin(LPWM_Output, PWM_CHANNEL_LPWM);
  
  // Initialize encoder pins
  pinMode(Encoder_output_A, INPUT_PULLUP);
  pinMode(Encoder_output_B, INPUT_PULLUP);
  
  // Attach interrupt for encoder
  attachInterrupt(digitalPinToInterrupt(Encoder_output_A), DC_Motor_Encoder, RISING);
  
  // Make sure we are stopped
  robot_stop();
  
  Serial.println("Motor control initialized with encoder");
  delay(1000); // Allow motor driver to initialize
}

void setMotorSpeed(int pwmValue, String direction) { 
  // Add dead-time to prevent shoot-through
  ledcWrite(PWM_CHANNEL_RPWM, 0);
  ledcWrite(PWM_CHANNEL_LPWM, 0);
  delayMicroseconds(100); // 100μs dead-time
  
  if (direction == "Forward") {
    ledcWrite(PWM_CHANNEL_LPWM, 0);
    ledcWrite(PWM_CHANNEL_RPWM, pwmValue);
  } else if (direction == "Reverse") {
    ledcWrite(PWM_CHANNEL_RPWM, 0);
    ledcWrite(PWM_CHANNEL_LPWM, pwmValue);
  } else if (direction == "Left") {
    // Short burst forward for left turn
    ledcWrite(PWM_CHANNEL_LPWM, 0);
    ledcWrite(PWM_CHANNEL_RPWM, pwmValue * 0.5);
    delay(100);
    ledcWrite(PWM_CHANNEL_RPWM, 0);
  } else if (direction == "Right") {
    // Short burst reverse for right turn
    ledcWrite(PWM_CHANNEL_RPWM, 0);
    ledcWrite(PWM_CHANNEL_LPWM, pwmValue * 0.5);
    delay(100);
    ledcWrite(PWM_CHANNEL_LPWM, 0);
  }
}

void robot_stop() {
  ledcWrite(PWM_CHANNEL_RPWM, 0);
  ledcWrite(PWM_CHANNEL_LPWM, 0);
}

void robot_fwd() {
  setMotorSpeed(motor_speed, "Forward");
}

void robot_back() {
  setMotorSpeed(motor_speed, "Reverse");
}

void robot_right() {
  setMotorSpeed(motor_speed, "Right");
}

void robot_left() {
  setMotorSpeed(motor_speed, "Left");
}

// === Encoder ISR ===
void IRAM_ATTR DC_Motor_Encoder() {
  int b = digitalRead(Encoder_output_B);
  if (b > 0) {
    encoderValue++;
  } else {
    encoderValue--;
  }
}

