#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

//needs mac of the motor esp so it can send controls over to it
uint8_t motorMAC[] = { 0xec, 0xe3, 0x34, 0x1b, 0x1e, 0x18 };

//make sure the wifi channels are all the same across the esps
const int WIFI_CHANNEL = 6;

//use these pins dont change them some of the other gpios don't have functionality to support ADC
//if you do want to change it then use only those labeled ADC in the pinout diagram
const int xPin = 35;
const int yPin = 34;
const int swPin = 19;

// the joystick's idle postion is unfortunately not 0,0 . idk which genius though that would be smart, but to work wround that
// i had to put an offset
const int X_CENTER = 1892;
const int Y_CENTER = 1886;

//this is to reduce noise because the joystick keep reading that its being moved/joysticked even though its not
//so you add a range like if its center + 700 dont move because its just jittery
const int DEADZONE = 1000;
//reduce deadzon = more sensitive but also higher chance of jitter and vice versa

const int MIN_SPEED = 90;
const int MAX_SPEED = 255;

const unsigned long SEND_INTERVAL = 100;
unsigned long lastSendTime = 0;

String lastCommand = "";

typedef struct {
  char command[32];
} RobotCommand;

//default code to use espnow
void onDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  Serial.print("Send status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

void sendCommand(String cmd) {
  RobotCommand msg;
  memset(&msg, 0, sizeof(msg));
  strncpy(msg.command, cmd.c_str(), sizeof(msg.command) - 1);

  esp_now_send(motorMAC, (uint8_t *)&msg, sizeof(msg));

  Serial.print("Sent: ");
  Serial.println(cmd);
}


//average out the joystick readings to further reduce noise
int readAverage(int pin) {
  long total = 0;

  for (int i = 0; i < 10; i++) {
    total += analogRead(pin);
    delay(2);
  }

  return total / 10;
}


//convert the movement of the joystick to actual direction and speed
int axisToSpeed(int value, int center) {
  int distance = abs(value - center);

  if (distance < DEADZONE) return 0;

  int maxDistance = max(center, 4095 - center);

  int speed = map(distance, DEADZONE, maxDistance, MIN_SPEED, MAX_SPEED);
  return constrain(speed, MIN_SPEED, MAX_SPEED);
}

//setup code - default
void setup() {
  Serial.begin(115200);

  pinMode(swPin, INPUT_PULLUP);

  WiFi.mode(WIFI_STA);
  esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);

  Serial.print("Joystick ESP MAC: ");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }

  esp_now_register_send_cb(onDataSent);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, motorMAC, 6);
  peerInfo.channel = WIFI_CHANNEL;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add motor peer");
    return;
  }

  Serial.println("Joystick ESP-NOW ready");
}

void loop() {
  if (millis() - lastSendTime < SEND_INTERVAL) return;
  lastSendTime = millis();

  //keep reading what the joystick says every fixed interval as defined above

  //based on the location of the joystick set the speed and direction ysing the axisToSpeed func.

  int x = readAverage(xPin);
  int y = readAverage(yPin);
  int button = digitalRead(swPin);

  int xOffset = x - X_CENTER;
  int yOffset = y - Y_CENTER;

  int xSpeed = axisToSpeed(x, X_CENTER);
  int ySpeed = axisToSpeed(y, Y_CENTER);

  int speed = max(xSpeed, ySpeed);

  bool right = xOffset < -DEADZONE;
  bool left  = xOffset > DEADZONE;

  bool up    = yOffset < -DEADZONE;
  bool down  = yOffset > DEADZONE;

  String cmd = "stop";

  //if you press button the it will rotate based on where josytick goes (left = CCW rotation and right = CW rotation)
  if (button == LOW) {
    if (right) {
      cmd = "rotCW";
    } else if (left) {
      cmd = "rotCCW";
    } else {
      cmd = "stop";
    }
  }

  //normal movement mode
  else {
    if (up && left) {
      cmd = "diagFL";
    } else if (up && right) {
      cmd = "diagFR";
    } else if (down && left) {
      cmd = "diagBL";
    } else if (down && right) {
      cmd = "diagBR";
    } else if (up) {
      cmd = "forward";
    } else if (down) {
      cmd = "backward";
    } else if (left) {
      cmd = "strafeR";
    } else if (right) {
      cmd = "strafeL";
    } else {
      cmd = "stop";
    }
  }

  String fullCommand;

  if (cmd == "stop") {
    fullCommand = "stop";
  } else {
    fullCommand = cmd + ":" + String(speed);
  }

  if (fullCommand != lastCommand || cmd != "stop") {
    sendCommand(fullCommand);
    lastCommand = fullCommand;
  }

  //debugging
  Serial.print("X: ");
  Serial.print(x);
  Serial.print("  Y: ");
  Serial.print(y);
  Serial.print("  SW: ");
  Serial.print(button);
  Serial.print("  CMD: ");
  Serial.println(fullCommand);
}