// ======================
// ESP32 ROVER CONTROLLER V2
// Dual Motor Control 
// ======================
#include <WiFi.h>
#include <LittleFS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

// === Motor Control Setup (IBT-2 Driver) ===
// Using safe GPIOs (avoid 6, 7, 8, 9, 10, 11)
// Motor Control Pins - Dual H-Bridge Setup
// Left Motor
#define LEFT_RPWM_PIN  5    // Left motor forward
#define LEFT_LPWM_PIN  4    // Left motor reverse


// Right Motor  
#define RIGHT_RPWM_PIN 26   // Right motor forward
#define RIGHT_LPWM_PIN 25   // Right motor reverse


// === Encoder Setup ===
#define LEFT_ENC_A     12
#define LEFT_ENC_B     13
#define RIGHT_ENC_A    14
#define RIGHT_ENC_B    27
#define PULSES_PER_ROTATION 3 // 3 pulses = 1 full rotation

// LEDC PWM Settings
#define PWM_FREQ       2000  // 20kHz for IBT-2
#define PWM_RESOLUTION 8     // 8-bit resolution (0-255)
#define PWM_CHANNEL_LF 0    // Left Forward
#define PWM_CHANNEL_LR 1    // Left Reverse
#define PWM_CHANNEL_RF 2    // Right Forward
#define PWM_CHANNEL_RR 3    // Right Reverse

// Web Server
AsyncWebServer server(80);

// WiFi Configuration
const char* ssid = "ROVER_V2";
const char* password = "pass123456";

// Motor Variables
volatile long leftEncoderValue = 0;
volatile long rightEncoderValue = 0;
int leftMotorSpeed = 0;   // -100 to 100
int rightMotorSpeed = 0;  // -100 to 100

// Safety and Control
unsigned long lastCommandTime = 0;
const unsigned long TIMEOUT_MS = 10000; // Stop motors if no command for 10 second
bool emergencyStop = false;

//Function declaration
//void setup();
void initMotorControl();
void stopAllMotors();
void handleSafety();

// Motor Control Class
class DCMotor {
private:
  int rpwmPin, lpwmPin, enablePin;
  int rpwmChannel, lpwmChannel;
  int currentSpeed;

public:
  DCMotor(int rpwm, int lpwm, int rpwmCh, int lpwmCh) {
    rpwmPin = rpwm;
    lpwmPin = lpwm;
    rpwmChannel = rpwmCh;
    lpwmChannel = lpwmCh;
    currentSpeed = 0;
  }

  void init() {
    // Setup PWM channels
    ledcSetup(rpwmChannel, PWM_FREQ, PWM_RESOLUTION);
    ledcSetup(lpwmChannel, PWM_FREQ, PWM_RESOLUTION);
    // Attach pins to PWM channels  
    ledcAttachPin(rpwmPin, rpwmChannel);
    ledcAttachPin(lpwmPin, lpwmChannel);
    
    stop();
  }

  void setSpeed(int speed) {
    // Constrain speed to -100 to 100
    speed = constrain(speed, -100, 100);
    currentSpeed = speed;
    
    if (emergencyStop) {
      speed = 0;
    }
    
    // Convert percentage to PWM value (0-255)
    int pwmValue = map(abs(speed), 0, 100, 0, 255);
    
    if (speed > 0) {
      // Forward
      ledcWrite(rpwmChannel, pwmValue);
      ledcWrite(lpwmChannel, 0);
    } else if (speed < 0) {
      // Reverse
      ledcWrite(rpwmChannel, 0);
      ledcWrite(lpwmChannel, pwmValue);
    } else {
      // Stop
      ledcWrite(rpwmChannel, 0);
      ledcWrite(lpwmChannel, 0);
    }
  }

  void stop() {
    setSpeed(0);
  }

  int getCurrentSpeed() {
    return currentSpeed;
  }
};


// Motor instances
DCMotor leftMotor(LEFT_RPWM_PIN, LEFT_LPWM_PIN, PWM_CHANNEL_LF, PWM_CHANNEL_LR);
DCMotor rightMotor(RIGHT_RPWM_PIN, RIGHT_LPWM_PIN, PWM_CHANNEL_RF, PWM_CHANNEL_RR);

// === Encoder ISR ===
void IRAM_ATTR leftEncoderISR() {
  if (digitalRead(LEFT_ENC_A) == digitalRead(LEFT_ENC_B)) {
    leftEncoderValue++;
  } else {
    leftEncoderValue--;
  }
}

void IRAM_ATTR rightEncoderISR() {
  if (digitalRead(RIGHT_ENC_A) == digitalRead(RIGHT_ENC_B)) {
    rightEncoderValue++;
  } else {
    rightEncoderValue--;
  }
}

// === Motor Control Functions ===
void initMotorControl() {
  Serial.println("Initializing motor control...");

  // Initialize motors
  leftMotor.init();
  rightMotor.init();
  
  // Initialize encoder pins
  pinMode(LEFT_ENC_A, INPUT_PULLUP);
  pinMode(LEFT_ENC_B, INPUT_PULLUP);
  pinMode(RIGHT_ENC_A, INPUT_PULLUP);
  pinMode(RIGHT_ENC_B, INPUT_PULLUP);
  
  // Attach interrupt for encoder
  attachInterrupt(digitalPinToInterrupt(LEFT_ENC_A), leftEncoderISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(RIGHT_ENC_A), rightEncoderISR, CHANGE);
  
  Serial.println("Motor control initialized");
  delay(1000); // Allow motor driver to initialize

}

void setup() {
  Serial.begin(9600);
  Serial.println("ESP32 Rover Controller V2 Starting...");

   // Initialize subsystems
  initMotorControl();

  // Start WiFi
  WiFi.softAP(ssid, password);
  Serial.println("ESP32 Access Point Started");
  Serial.print("AP IP: "); Serial.println(WiFi.softAPIP());
  
  if (!LittleFS.begin()) {
    Serial.println("LittleFS Mount Failed");
    // LittleFS.format();
    // ESP.restart();
    //return;
  } else {
    Serial.println("LittleFS Mounted Successfully");
    Serial.println("LittleFS contents:");
    File root = LittleFS.open("/");
    File file = root.openNextFile();
    while(file){
      Serial.printf("File: %s\n", file.name());
      file = root.openNextFile();
    }
  }
  
  // More robust version:
server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
  request->send(LittleFS, "/car.html", "text/html");
});

server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
  request->send(LittleFS, "/style.css", "text/css");
});

server.on("/main.js", HTTP_GET, [](AsyncWebServerRequest *request){
  request->send(LittleFS, "/main.js", "application/javascript");
});


  // Motor control handler
server.on("/motors", HTTP_GET, [](AsyncWebServerRequest *request){
  if (request->hasParam("left") && request->hasParam("right")) {
    // Constrain inputs to -100 to 100
    int leftValue = constrain(request->getParam("left")->value().toInt(), -100, 100);
    int rightValue = constrain(request->getParam("right")->value().toInt(), -100, 100);
    
    // Atomic update of both motors
    leftMotorSpeed = leftValue;
    rightMotorSpeed = rightValue;
    leftMotor.setSpeed(leftValue);
    rightMotor.setSpeed(rightValue);

    Serial.printf("[MOTORS] L:%3d R:%3d | Encoder L:%6ld R:%6ld\n", 
                 leftValue, rightValue, leftEncoderValue, rightEncoderValue);
    
    request->send(200, "text/plain", 
      String("Motors set: L") + leftValue + " R" + rightValue);
    lastCommandTime = millis();
  } else {
    request->send(400, "text/plain", "Missing parameters");
  }
});

// Stop handler
server.on("/stop", HTTP_GET, [](AsyncWebServerRequest *request){
  stopAllMotors();
  request->send(200, "text/plain", "All motors stopped");
  lastCommandTime = millis();
});

// Status handler (optional but useful)
server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request){
  String json = "{";
  json += "\"leftSpeed\":" + String(leftMotorSpeed) + ",";
  json += "\"rightSpeed\":" + String(rightMotorSpeed) + ",";
  json += "\"leftEncoder\":" + String(leftEncoderValue) + ",";
  json += "\"rightEncoder\":" + String(rightEncoderValue);
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

  // Start server
  server.begin();
  Serial.println("HTTP server started on port 80");

}
void loop() {
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 1000) {
    lastPrint = millis();
    Serial.printf("Current Speeds: L:%d R:%d\n", 
                 leftMotor.getCurrentSpeed(), 
                 rightMotor.getCurrentSpeed());
  }
  // Safety checks
  //handleSafety();

}


void stopAllMotors() {
  leftMotor.stop();
  rightMotor.stop();
  leftMotorSpeed = 0;
  rightMotorSpeed = 0;
  Serial.println("All motors stopped");
}

void handleSafety() {
  // Check for command timeout
  if (millis() - lastCommandTime > TIMEOUT_MS && (leftMotorSpeed != 0 || rightMotorSpeed != 0)) {
    Serial.println("Command timeout - stopping motors for safety");
    stopAllMotors();
  }
}