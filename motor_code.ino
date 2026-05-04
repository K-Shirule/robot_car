#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

unsigned long lastCommandTime = 0;
const unsigned long COMMAND_TIMEOUT = 700; //delay so that it auto-stops in case the last signal is stucj on a direction - basically the robot keeps moving even though it 
//has been told to stop

bool isMoving = false;

const int DEFAULT_SPEED = 200;  // 0-255

// Direction pins
int motor1Pin1 = 27, motor1Pin2 = 14; //motors have been labeled on the bottom side of the robot so look there
int motor2Pin1 = 33, motor2Pin2 = 32;
int motor3Pin1 = 21, motor3Pin2 = 19;
int motor4Pin1 = 23, motor4Pin2 = 22;


//enable pins to control the speed as well as direction - can be commented out if we go with the fixed speed implementation
int motor1EN = 13;
int motor2EN = 12;
int motor3EN = 5;
int motor4EN = 18;

//the frequency of the PWM signal and we need 8 bits since we want to represent speed from 0 all the way to 255
const int PWM_FREQ = 1000;
const int PWM_RESOLUTION = 8; // 0-255

typedef struct {
  char command[32];
} RobotCommand;

//this is where the speed is actually set into the enable pin
void setMotorSpeed(int speed) {
  speed = constrain(speed, 0, 255);

  ledcWrite(motor1EN, speed);
  ledcWrite(motor2EN, speed);
  ledcWrite(motor3EN, speed);
  ledcWrite(motor4EN, speed);
}

void setSingleMotorSpeed(int enPin, int speed) {
  speed = constrain(speed, 0, 255);
  ledcWrite(enPin, speed);
}

//motor rotation functons - CW = clockwise :: CCW = counter clockwise
//these are individual motro functions - dont really move the robot just the individual motor
void motor1CW(int speed) {
  digitalWrite(motor1Pin1, LOW);
  digitalWrite(motor1Pin2, HIGH);
  setSingleMotorSpeed(motor1EN, speed);
}

void motor1CCW(int speed) {
  digitalWrite(motor1Pin1, HIGH);
  digitalWrite(motor1Pin2, LOW);
  setSingleMotorSpeed(motor1EN, speed);
}

void motor2CW(int speed) {
  digitalWrite(motor2Pin1, HIGH);
  digitalWrite(motor2Pin2, LOW);
  setSingleMotorSpeed(motor2EN, speed);
}

void motor2CCW(int speed) {
  digitalWrite(motor2Pin1, LOW);
  digitalWrite(motor2Pin2, HIGH);
  setSingleMotorSpeed(motor2EN, speed);
}

void motor3CW(int speed) {
  digitalWrite(motor3Pin1, HIGH);
  digitalWrite(motor3Pin2, LOW);
  setSingleMotorSpeed(motor3EN, speed);
}

void motor3CCW(int speed) {
  digitalWrite(motor3Pin1, LOW);
  digitalWrite(motor3Pin2, HIGH);
  setSingleMotorSpeed(motor3EN, speed);
}

void motor4CW(int speed) {
  digitalWrite(motor4Pin1, HIGH);
  digitalWrite(motor4Pin2, LOW);
  setSingleMotorSpeed(motor4EN, speed);
}

void motor4CCW(int speed) {
  digitalWrite(motor4Pin1, LOW);
  digitalWrite(motor4Pin2, HIGH);
  setSingleMotorSpeed(motor4EN, speed);
}

void stopMotors() {
  digitalWrite(motor1Pin1, LOW); digitalWrite(motor1Pin2, LOW);
  digitalWrite(motor2Pin1, LOW); digitalWrite(motor2Pin2, LOW);
  digitalWrite(motor3Pin1, LOW); digitalWrite(motor3Pin2, LOW);
  digitalWrite(motor4Pin1, LOW); digitalWrite(motor4Pin2, LOW);

  setMotorSpeed(0);
  isMoving = false;
}

//this is to prevent that glitch where it keeps moving even though i tell it to fking stop
void markMoving() {
  lastCommandTime = millis();
  isMoving = true;
}

// motor1 = front-left
// motor2 = front-right
// motor3 = rear-left
// motor4 = rear-right

//actual meaninful movement of the robot
void moveForward(int speed) {
  markMoving();
  motor1CW(speed);
  motor2CCW(speed);
  motor3CW(speed);
  motor4CCW(speed);
}

void moveBackward(int speed) {
  markMoving();
  motor1CCW(speed);
  motor2CW(speed);
  motor3CCW(speed);
  motor4CW(speed);
}

void strafeLeft(int speed) { //doesnt rotate only moves right while looking in one direction like fortnite
  markMoving();
  motor1CCW(speed);
  motor2CCW(speed);
  motor3CW(speed);
  motor4CW(speed);
}

void strafeRight(int speed) {
  markMoving();
  motor1CW(speed);
  motor2CW(speed);
  motor3CCW(speed);
  motor4CCW(speed);
}

void diagonalFrontLeft(int speed) {
  markMoving();
  stopMotors();
  isMoving = true;

  motor2CCW(speed);
  motor3CW(speed);
}

void diagonalFrontRight(int speed) {
  markMoving();
  stopMotors();
  isMoving = true;

  motor1CW(speed);
  motor4CCW(speed);
}

void diagonalBackLeft(int speed) {
  markMoving();
  stopMotors();
  isMoving = true;

  motor1CCW(speed);
  motor4CW(speed);
}

void diagonalBackRight(int speed) {
  markMoving();
  stopMotors();
  isMoving = true;

  motor2CW(speed);
  motor3CCW(speed);
}

void rotateCCW(int speed) {
  markMoving();
  motor1CW(speed);
  motor2CW(speed);
  motor3CW(speed);
  motor4CW(speed);
}

void rotateCW(int speed) {
  markMoving();
  motor1CCW(speed);
  motor2CCW(speed);
  motor3CCW(speed);
  motor4CCW(speed);
}

//the below two functions just help us get the desired direction and speed by converting the command recieved from the 3rd esp32 which
//sends it in the format -> forwardR : 200 (sepearte at the colon)

int extractSpeed(String cmd) {
  int colonIndex = cmd.indexOf(':');

  if (colonIndex == -1) {
    return DEFAULT_SPEED;
  }

  int speed = cmd.substring(colonIndex + 1).toInt();
  return constrain(speed, 0, 255);
}

String extractCommand(String cmd) {
  int colonIndex = cmd.indexOf(':');

  if (colonIndex == -1) {
    return cmd;
  }

  return cmd.substring(0, colonIndex);
}

// ESP-NOW receive callback - default code - bestfriend helped me here
void onReceive(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  RobotCommand msg;
  memset(&msg, 0, sizeof(msg));

  int copyLen = min(len, (int)sizeof(msg.command) - 1);
  memcpy(msg.command, data, copyLen);
  msg.command[copyLen] = '\0';

  String rawCmd = String(msg.command);
  String cmd = extractCommand(rawCmd);
  int speed = extractSpeed(rawCmd);

  Serial.print("CMD: ");
  Serial.print(cmd);
  Serial.print(" | Speed: ");
  Serial.println(speed);

  if      (cmd == "forward")   moveForward(speed);
  else if (cmd == "backward")  moveBackward(speed);
  else if (cmd == "strafeL")   strafeLeft(speed);
  else if (cmd == "strafeR")   strafeRight(speed);
  else if (cmd == "diagFL")    diagonalFrontLeft(speed);
  else if (cmd == "diagFR")    diagonalFrontRight(speed);
  else if (cmd == "diagBL")    diagonalBackLeft(speed);
  else if (cmd == "diagBR")    diagonalBackRight(speed);
  else if (cmd == "rotCW")     rotateCW(speed);
  else if (cmd == "rotCCW")    rotateCCW(speed);
  else if (cmd == "stop")      stopMotors();
  else {
    Serial.println("Unknown command");
    stopMotors();
  }
}

//this makes all the pins on and stuff similar to 146 labs where we enable gpios and power buses
void setup() {
  Serial.begin(115200);

  pinMode(motor1Pin1, OUTPUT); pinMode(motor1Pin2, OUTPUT);
  pinMode(motor2Pin1, OUTPUT); pinMode(motor2Pin2, OUTPUT);
  pinMode(motor3Pin1, OUTPUT); pinMode(motor3Pin2, OUTPUT);
  pinMode(motor4Pin1, OUTPUT); pinMode(motor4Pin2, OUTPUT);

  pinMode(motor1EN, OUTPUT);
  pinMode(motor2EN, OUTPUT);
  pinMode(motor3EN, OUTPUT);
  pinMode(motor4EN, OUTPUT);

  ledcAttach(motor1EN, PWM_FREQ, PWM_RESOLUTION);
  ledcAttach(motor2EN, PWM_FREQ, PWM_RESOLUTION);
  ledcAttach(motor3EN, PWM_FREQ, PWM_RESOLUTION);
  ledcAttach(motor4EN, PWM_FREQ, PWM_RESOLUTION);

  stopMotors();

  //defualt code - again help from best friend
  WiFi.mode(WIFI_STA);

  esp_wifi_set_channel(6, WIFI_SECOND_CHAN_NONE);

  Serial.print("Motor MAC: ");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }

  if (esp_now_register_recv_cb(onReceive) != ESP_OK) {
    Serial.println("Failed to register receive callback");
    return;
  }

  Serial.println("ESP-NOW ready");
}

void loop() {
  if (isMoving && millis() - lastCommandTime > COMMAND_TIMEOUT) {
    stopMotors();
    Serial.println("Timeout stop");
  }

  delay(5);
}