#include <Arduino.h>
#include <Keypad.h>
#include <WiFi.h>
#include <HTTPClient.h>

// -------------------------------------------
// CONFIGURATION
// -------------------------------------------
// GPIO pins where the PIR sensor, RGB LED and 4x3 matrix keypad are connected
// -------------------------------------------
#define PIR_SENSOR_PIN 15
#define RGB_LED_RED 13
#define RGB_LED_GREEN 12
#define RGB_LED_BLUE 14

// -------------------------------------------
// 3x4 matrix keypad
// -------------------------------------------
const byte ROWS = 4; // four rows
const byte COLS = 3; // three columns
// ------
// Pins
// ------
// 1  2  3  4  5  6  7
// 33 25 26 21 19 18 5
byte rowPins[ROWS] = {25, 5, 18, 21};
byte colPins[COLS] = {26, 33, 19};

// -------------------------------------------
// WiFi
// -------------------------------------------
const char *ssid = "YOUR_WIFI_SSID";
const char *password = "YOUR_WIFI_PASSWORD";
const char *hostname = "home-alarm";

// -------------------------------------------
// HTTPS server
// -------------------------------------------
const char *serverURL = "YOUR_API_SERVER_URL";
const char *serverToken = "YOUR_API_SERVER_TOKEN";

// -------------------------------------------
// Alarm PIN
// -------------------------------------------
// how much time the user has to type the PIN after the alarm is triggered
// we add 2 more seconds here compared to the server side
// just to make sure the new trigger is not resetting the old one
// default: 32000 (32 seconds)
#define ALARM_TRIGGER_TIME_WINDOW 32000
// when arming the system, wait for a bit so the user can leave the room
// default: 15000 (15 seconds)
#define ARMED_DELAY 15000
// if the user does not arm the system in this time, the system will arm itself
// default: 720 (12 hours)
#define AUTO_ARM_MINUTES 720
const char *alarmPin = "YOUR_ALARM_PIN"; // eg 1234#

// -------------------------------------------
// END CONFIGURATION
// -------------------------------------------

char keys[ROWS][COLS] = {
    {'1', '2', '3'},
    {'4', '5', '6'},
    {'7', '8', '9'},
    {'*', '0', '#'}};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// Variable to store the PIR sensor status
bool armed = false;
bool triggered = false;
unsigned long triggerTime = 0;
unsigned long autoArmTime = 0;
unsigned long lastAutoArmUpdateTime = 0;
// if the system detects motion while it's still not armed
// update the autoArmTime to the current time but only once every minute
const int updateAutoArmTime = 60000;
String pinBuffer = "";
bool standby = false;
// for controlling the green LED brightness
const int greenPWMChannel = 1;
const int freq = 5000;
const int resolution = 8;

// Set the color of the RGB LED
// If only green is true, set the LED to low brightness
// We do this because the green LED is very bright at night
void setLEDColor(bool red, bool green, bool blue, bool lowGreenBrightness = false)
{
  digitalWrite(RGB_LED_RED, red);
  // for green, we control the brightness
  if (green)
  {
    if (lowGreenBrightness)
    {
      ledcWrite(greenPWMChannel, 5);
    }
    else
    {
      ledcWrite(greenPWMChannel, 255);
    }
  }
  else
  {
    ledcWrite(greenPWMChannel, 0);
  }

  digitalWrite(RGB_LED_BLUE, blue);
}

// Check if connected to WiFi, if not, try to connect
void checkInitWiFi()
{
  // make sure WiFi is connected
  // we will run the init inside the loop, in case the connection is lost
  if (WiFi.status() == WL_CONNECTED)
  {
    return;
  }
  WiFi.mode(WIFI_STA);
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
  WiFi.setHostname(hostname);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED)
  {
    setLEDColor(true, false, true);
    Serial.print('.');
    delay(100);
    setLEDColor(false, false, false);
    delay(100);
  }

  Serial.println("WiFi connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.print("HostName: ");
  Serial.println(WiFi.getHostname());
  Serial.print("RRSI: ");
  Serial.println(WiFi.RSSI());
  setLEDColor(false, true, false, true);
}

// Send HTTP(s) GET request
void sendGETRequest(String url)
{
  HTTPClient http;
  int responseCode;
  String responseBody;

  // based on URL protocol use HTTP or HTTPS
  if (url.startsWith("https://"))
  {
    WiFiClientSecure client;
    client.setInsecure();

    http.begin(client, url); // HTTPS connection
    responseCode = http.GET();
  }
  else if (url.startsWith("http://"))
  {
    WiFiClient client;
    http.begin(client, url); // HTTP connection
    responseCode = http.GET();
  }
  // end connection and return response
  http.end();
  Serial.print("HTTP response code: ");
  Serial.println(responseCode);
}

// Activate the alarm, but slowly, so the user can leave the room
void activate()
{
  int counter = 0;
  int dynamicDelay = 500;
  // blink the LED in blue
  Serial.println("Activating alarm in " + String(ARMED_DELAY / 1000) + " seconds");
  while (counter < ARMED_DELAY)
  {
    setLEDColor(true, false, false);
    delay(dynamicDelay);
    setLEDColor(false, false, false);
    delay(dynamicDelay);
    counter += dynamicDelay * 2;
    // decrease the delay each time, but not more than 100ms
    if (dynamicDelay > 50)
    {
      dynamicDelay -= 25;
    }
  }
  armed = true;
  Serial.println("Armed");
  setLEDColor(true, false, false);
  pinBuffer = "";
  // send request to server that tells the alarm was activated
  String url = String(serverURL) + "/activate?token=" + String(serverToken);
  sendGETRequest(url);
}

// PIN was typed correctly, reset everything and send a request to the server
void pinTypedCorrectly()
{
  // blink green 3 times
  setLEDColor(false, true, false);
  delay(100);
  setLEDColor(false, false, false);
  delay(100);
  setLEDColor(false, true, false);
  delay(100);
  setLEDColor(false, false, false);
  delay(100);
  setLEDColor(false, true, false);
  delay(100);
  setLEDColor(false, false, false);
  delay(100);
  setLEDColor(false, true, false, true);
  Serial.println("PIN verified, alarm stopped");
  triggered = false;
  armed = false;
  triggerTime = 0;
  pinBuffer = "";
  standby = false;
  autoArmTime = millis();
  // send request to server that tells the alarm was stopped by the owner
  // this will make the server stop the notification process
  String url = String(serverURL) + "/stop?token=" + String(serverToken);
  sendGETRequest(url);
}

void setup()
{
  // Start the serial communication to monitor the sensor status
  Serial.begin(115200);

  // PIR sensor pin as input
  pinMode(PIR_SENSOR_PIN, INPUT);

  // RGB LED pins as output
  pinMode(RGB_LED_RED, OUTPUT);

  // we handle the green LED with PWM, so we can control the brightness
  ledcSetup(greenPWMChannel, freq, resolution);
  ledcAttachPin(RGB_LED_GREEN, greenPWMChannel);

  pinMode(RGB_LED_BLUE, OUTPUT); // We won't use blue in this case

  // set the led to all 3 colors to know it's working
  setLEDColor(true, false, false);
  delay(250);
  setLEDColor(false, true, false);
  delay(250);
  setLEDColor(false, false, true);
  delay(250);
  setLEDColor(false, false, false);
  delay(250);
}

void loop()
{
  // check if the WiFi is connected
  checkInitWiFi();

  // initially, we're not armed and not triggered
  // if the user presses the * key, we'll arm the system
  char key = keypad.getKey();

  // used for debugging
  if (key)
  {
    Serial.print("Key pressed: ");
    Serial.println(key);
  }

  // autoArmTime and lastAutoArmUpdateTime are dependent on millis()
  // but millis resets after ~40 days, so we need to handle the overflow
  if (millis() < autoArmTime || millis() < lastAutoArmUpdateTime)
  {
    // Handle the overflow by resetting autoArmTime
    // This assumes that an overflow has occurred, so we reset autoArmTime to millis()
    Serial.println("millis() overflow detected, resetting autoArmTime");
    autoArmTime = millis();
    lastAutoArmUpdateTime = millis();
  }

  // check if alarm is triggered
  if (triggered)
  {
    // if the alarm is triggered, and the time window has passed, reset the alarm
    if (millis() - triggerTime > ALARM_TRIGGER_TIME_WINDOW)
    {
      triggered = false;
      triggerTime = 0;
      pinBuffer = "";
      standby = false;
      setLEDColor(true, false, false);
      Serial.println("Alarm reset");
      return;
    }

    // if no key was typed, return
    if (!key)
    {
      return;
    }

    // if a key was typed, add it to the buffer and compare it with the alarm pin
    pinBuffer += key;

    // if the buffer has more than 100 characters, keep only the last 50
    // probably a bit overkill, but just to make sure it's in the user range
    // and also to avoid memory issues
    if (pinBuffer.length() > 100)
    {
      pinBuffer = pinBuffer.substring(50);
    }

    // check if the buffer contains the alarm pin
    if (pinBuffer.endsWith(alarmPin))
    {
      pinTypedCorrectly();
    }
    else
    {
      setLEDColor(false, false, false);
      delay(50);
      setLEDColor(false, false, true);
    }
    return;
  }

  // -------------------------------------------
  // Read the PIR sensor value
  // -------------------------------------------
  bool motionDetected = digitalRead(PIR_SENSOR_PIN);

  // if we are not armed, check for key presses
  if (!armed)
  {
    // the user can only arm the system by pressing the * key
    if (key == '*')
    {
      activate();
    }
    // if a different key is pressed, blink the LED
    else if (key)
    {
      setLEDColor(false, false, false);
      delay(50);
      setLEDColor(false, true, false);
    }

    // if the user does not arm the system after a certain time, the system will arm itself
    if (millis() - autoArmTime > AUTO_ARM_MINUTES * 60000)
    {
      Serial.println("Automatically activating the alarm after " + String(AUTO_ARM_MINUTES) + " minutes");
      activate();
    }

    // if there is motion detected, reset the autoArmTime
    // this way the system will not arm itself if the user is in the room
    if (motionDetected && millis() - lastAutoArmUpdateTime > updateAutoArmTime)
    {
      Serial.println("Motion detected while not armed, resetting auto arm time");
      autoArmTime = millis();
      lastAutoArmUpdateTime = millis();
    }
    return;
  }

  // Check if motion is detected
  if (motionDetected)
  {
    triggered = true;
    triggerTime = millis();
    // send request to server
    // we don't care about the response, we just want to trigger the alarm on the server side
    // the server will send a notification to the user instead of the alarm sending it
    // this way we know for sure if somebody is cutting the power to the alarm
    // the notification will be sent to the user regardless of the alarm having power or not
    String url = String(serverURL) + "/trigger?token=" + String(serverToken);
    sendGETRequest(url);
    setLEDColor(false, false, true);
    Serial.println("Alarm triggered");
  }
  else if (!standby)
  {
    // we do it with standby to avoid resetting the LED color to green on each loop
    Serial.println("No motion detected - standby");
    setLEDColor(true, false, false);
    standby = true;
  }
}
