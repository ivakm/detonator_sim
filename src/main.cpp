#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>

#define ARMLED 25
#define FIRELED 27
#define BUZZER 26
#define BUTTON 32
#define PWM_IN 34

#define ARMING 10000
#define ARMTIMEOUT 40000 // 10 sec for STATE_ARMING & 30 sec for STATE_ARM

WebServer server(80);

typedef enum
{
  STATE_SAFE,
  STATE_ARMING,
  STATE_ARM,
  STATE_FIRE
} DetonatorState;

void system_init();

String getHtml();
void browser_init();

void detonator_update();
void detonator_set_state(DetonatorState newState);
void detonator_start();
void detonator_stop();
void detonator_fire();
void detonator_failsafe(const String &message);

DetonatorState currentState = STATE_SAFE;

unsigned long startTime = millis();

bool isReset = false;
int armBeepCount = 0;

unsigned long lastBlink = 0;

void setup()
{
  Serial.begin(115200);

  pinMode(ARMLED, OUTPUT);
  pinMode(FIRELED, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(BUTTON, INPUT_PULLUP);
  pinMode(PWM_IN, INPUT);

  system_init();
  browser_init();
}

void loop()
{
  detonator_update();
  server.handleClient();

  int pwmValue = pulseIn(PWM_IN, HIGH, 25000);

  if (pwmValue > 0)
  {
    if (pwmValue >= 900 && pwmValue <= 1100)
    {
      detonator_stop();
    }
    else if (pwmValue >= 1900 && pwmValue <= 2100)
    {
      detonator_fire();
    }
    else
    {
      detonator_failsafe("Invalid PWM value: " + String(pwmValue));
    }
  }

  if (digitalRead(BUTTON) == LOW)
  {
    detonator_start();
  }
}

void system_init()
{
  uint64_t chipId = ESP.getEfuseMac();
  if (!chipId)
  {
    detonator_failsafe("System init failed: invalid chip ID");
    return;
  }

  Serial.printf("Chip ID: %04X%08X\n", (uint32_t)(chipId >> 32), (uint32_t)chipId);

  if (digitalRead(BUTTON) == LOW)
  {
    detonator_failsafe("System init failed: button is stuck");
    return;
  }

  if (!SPIFFS.begin(true))
  {
    detonator_failsafe("System init failed: SPIFFS mount failed");
    return;
  }

  Serial.println("SPIFFS OK");

  digitalWrite(ARMLED, HIGH);
  digitalWrite(FIRELED, HIGH);
  digitalWrite(BUZZER, HIGH);
  delay(500);
  digitalWrite(ARMLED, LOW);
  digitalWrite(FIRELED, LOW);
  digitalWrite(BUZZER, LOW);

  Serial.println("System init OK");
}

String getHtml()
{
  return R"(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <style>
    body { background: #1a1a1a; color: #fff; font-family: sans-serif; text-align: center; padding: 50px; }
    h1 { color: #ff4444; }
    button { padding: 15px 30px; margin: 10px; font-size: 18px; border: none; border-radius: 8px; cursor: pointer; }
    a:nth-child(1) button { background: #44aa44; }
    a:nth-child(2) button { background: #aaaaaa; }
    a:nth-child(3) button { background: #ff4444; }
  </style>
</head>
<body>
  <h1>Detonator Control</h1>
  <a href="/start"><button>Start</button></a>
  <a href="/stop"><button>Stop</button></a>
  <a href="/fire"><button>Fire</button></a>
</body>
</html>
)";
}

void browser_init()
{
  WiFi.softAP("Detonator", "12345678");

  server.on("/", []()
            { server.send(200, "text/html", getHtml()); });

  server.on("/start", []()
            {
    detonator_start();
    server.send(200, "text/html", getHtml()); });

  server.on("/stop", []()
            {
    detonator_stop();
    server.send(200, "text/html", getHtml()); });

  server.on("/fire", []()
            {
    detonator_fire();
    server.send(200, "text/html", getHtml()); });

  server.begin();
}

void detonator_update()
{
  switch (currentState)
  {
  case STATE_SAFE:
    if (!isReset)
    {
      digitalWrite(ARMLED, LOW);
      digitalWrite(FIRELED, LOW);
      digitalWrite(BUZZER, LOW);
      isReset = true;
      armBeepCount = 0;
    }

    break;

  case STATE_ARMING:
    if (millis() - lastBlink >= 500)
    {
      digitalWrite(ARMLED, !digitalRead(ARMLED));
      lastBlink = millis();
    }

    if (millis() - startTime >= ARMING)
    {
      detonator_set_state(STATE_ARM);
    }
    break;

  case STATE_ARM:
    digitalWrite(ARMLED, LOW);

    if (armBeepCount < 6 && millis() - lastBlink >= 500)
    {

      digitalWrite(BUZZER, !digitalRead(BUZZER));
      lastBlink = millis();
      armBeepCount++;
    }

    digitalWrite(FIRELED, HIGH);

    if (millis() - startTime >= ARMTIMEOUT)
    {
      detonator_failsafe("STATE_ARM - Timeout");
    }

    break;

  case STATE_FIRE:
    digitalWrite(FIRELED, LOW);
    digitalWrite(BUZZER, HIGH);
    delay(500);
    digitalWrite(BUZZER, LOW);
    detonator_set_state(STATE_SAFE);
    break;
  }
}

void detonator_set_state(DetonatorState newState)
{
  currentState = newState;
}

void detonator_start()
{
  if (currentState != STATE_SAFE)
  {
    Serial.print("You cannot start at the moment, you must be in STATE_SAFE state\n");
    return;
  }

  startTime = millis();
  detonator_set_state(STATE_ARMING);
}

void detonator_stop()
{
  if (currentState != STATE_ARMING && currentState != STATE_ARM)
  {
    Serial.print("You cannot stop at the moment, you must be in STATE_ARMING or STATE_ARM state\n");
    return;
  }

  isReset = false;
  detonator_set_state(STATE_SAFE);
}

void detonator_fire()
{
  if (currentState != STATE_ARM)
  {
    Serial.print("You cannot fire at the moment, you must be in STATE_ARM state\n");
    return;
  }

  detonator_set_state(STATE_FIRE);
}

void detonator_failsafe(const String &message)
{
  Serial.println("ERROR: " + message + "\n");
  detonator_set_state(STATE_SAFE);
}