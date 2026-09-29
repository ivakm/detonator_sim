#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

#define ARMLED 25
#define FIRELED 27
#define BUZZER 26
#define BUTTON 32

WebServer server(80);

String getHtml();
void browser_init();

typedef enum
{
  STATE_SAFE,
  STATE_ARMING,
  STATE_ARM,
  STATE_FIRE
} DetonatorState;

DetonatorState currentState = STATE_SAFE;

void detonator_update();
void detonator_set_state(DetonatorState newState);
void detonator_start();
void detonator_stop();
void detonator_fire();

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

  browser_init();
}

void loop()
{
  detonator_update();
  server.handleClient();

  if (digitalRead(BUTTON) == LOW)
  {
    detonator_start();
  }
}

String getHtml()
{
  return R"(
<!DOCTYPE html>
<html>
<body>
  <h1>Detonator Control</h1>
  <a href="/start"><button>Start</button></a>
  <a href="/stop"><button>Stop</button></a>
  <a href="/fire"><button>Підрив</button></a>
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

    if (millis() - startTime >= 10000) // 5 * 60 * 1000
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
    return;
  }

  startTime = millis();
  detonator_set_state(STATE_ARMING);
}

void detonator_stop()
{
  if (currentState != STATE_ARMING && currentState != STATE_ARM)
  {
    // Create Error message, like you cannot fire at the moment, you must be in STATE_ARM state
    return;
  }

  isReset = false;
  detonator_set_state(STATE_SAFE);
}

void detonator_fire()
{
  if (currentState != STATE_ARM)
  {
    // Create Error message, like you cannot fire at the moment, you must be in STATE_ARM state
    return;
  }

  detonator_set_state(STATE_FIRE);
}