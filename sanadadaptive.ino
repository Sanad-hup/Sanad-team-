
#include <Mouse.h>
#include <Keyboard.h>

// = الجويستيك =
const int horzPin = A2;
const int vertPin = A0;
const int selPin  = 9;

int vertZero, horzZero;
int vertValue, horzValue;
int mouseClickFlag = 0;

// = حساس الضغط =
const int pressurePin = A1;
int centerValue = 512;
float filteredPressure = 0;

// = إعدادات الحركة =
int deadZone = 12;
float maxSpeed = 4.5;
const unsigned long moveInterval = 10;

float accX = 0, accY = 0;
unsigned long lastMoveTime = 0;

// = عتبات النفخ والشفط =
float blowThreshold   = 7.0;      // قابلة للتحديث من Raspberry Pi
const int blowRelease     = 3;
float inhaleThreshold = 6.0;      // قابلة للتحديث من Raspberry Pi
const int inhaleRelease   = 3;

// = استقبال أوامر Adaptive Thresholds =
const float THRESH_MIN = 4.0;
const float THRESH_MAX = 200.0;

char cmdBuf[24];
byte cmdIndex = 0;

// = التوقيتات =
const unsigned long longBlowTime  = 450;
const unsigned long doubleGap     = 700;
const unsigned long keyboardGap   = 300;
const unsigned long clickInterval = 250;

// = إعدادات السكرول =
const int scrollThreshold     = 120;
const int scrollFastThreshold = 300;
const unsigned long scrollSlow = 150;
const unsigned long scrollFast = 60;
const bool invertScroll = false;

// = إرسال البيانات للـ Raspberry Pi =
const unsigned long telemetryInterval = 100;
unsigned long lastTelemetryTime = 0;

// = الحالات =
bool blowActive = false, longBlowActive = false;
unsigned long blowStartTime = 0, lastBlowEnd = 0;
int blowCount = 0;

bool inhaleActive = false;
unsigned long lastInhaleEnd = 0;
int inhaleCount = 0;

bool keyboardMode = false;

unsigned long lastClickTime = 0, lastScrollTime = 0;

void setup() {
  Serial.begin(9600);

  pinMode(horzPin, INPUT);
  pinMode(vertPin, INPUT);
  pinMode(selPin, INPUT_PULLUP);

  Mouse.begin();
  Keyboard.begin();

  vertZero = analogRead(vertPin);
  horzZero = analogRead(horzPin);

  delay(500);

  long sum = 0;
  for (int i = 0; i < 50; i++) {
    sum += analogRead(pressurePin);
    delay(10);
  }

  centerValue = sum / 50;
  filteredPressure = centerValue;

  Serial.println("INFO,SANAD_DEVICE_READY");
  Serial.println("HEADER,timestamp,joystickX,joystickY,pressure,baseline,blow,inhale");

  Serial.print("INFO,CENTER_VALUE,");
  Serial.println(centerValue);
}

void sendEvent(const char *name) {
  Serial.print("EVENT,");
  Serial.print(millis());
  Serial.print(",");
  Serial.println(name);
}

// تأكيد استلام العتبة الجديدة
void sendThresholdAck(const char *name, float value) {
  Serial.print("EVENT,");
  Serial.print(millis());
  Serial.print(",");
  Serial.print(name);
  Serial.print(",");
  Serial.println(value, 2);
}

// تنفيذ الأمر بعد اكتماله
void processCommand(char *cmd) {

  if (strncmp(cmd, "SET_BLOW ", 9) == 0) {

    float v = atof(cmd + 9);

    if (v >= THRESH_MIN && v <= THRESH_MAX) {

      blowThreshold = v;

      sendThresholdAck("BLOW_THRESHOLD_SET", blowThreshold);

    } else {

      sendThresholdAck("BLOW_THRESHOLD_REJECTED", v);
    }
  }

  else if (strncmp(cmd, "SET_INHALE ", 11) == 0) {

    float v = atof(cmd + 11);

    if (v >= THRESH_MIN && v <= THRESH_MAX) {

      inhaleThreshold = v;

      sendThresholdAck("INHALE_THRESHOLD_SET", inhaleThreshold);

    } else {

      sendThresholdAck("INHALE_THRESHOLD_REJECTED", v);
    }
  }

  else if (strncmp(cmd, "GET_THRESHOLDS", 14) == 0) {

    sendThresholdAck("BLOW_THRESHOLD", blowThreshold);
    sendThresholdAck("INHALE_THRESHOLD", inhaleThreshold);
  }
}

// قراءة غير حاجبة — لا تؤثر على الـ loop ولا على الجويستيك
void handleSerialCommands() {

  while (Serial.available() > 0) {

    char c = Serial.read();

    if (c == '\n' || c == '\r') {

      if (cmdIndex > 0) {

        cmdBuf[cmdIndex] = '\0';

        processCommand(cmdBuf);

        cmdIndex = 0;
      }
    }

    else if (cmdIndex < sizeof(cmdBuf) - 1) {

      cmdBuf[cmdIndex++] = c;
    }

    else {

      cmdIndex = 0;
    }
  }
}

void sendTelemetry(unsigned long now) {

  Serial.print("DATA,");
  Serial.print(now);
  Serial.print(",");
  Serial.print(horzValue);
  Serial.print(",");
  Serial.print(vertValue);
  Serial.print(",");
  Serial.print(filteredPressure, 2);
  Serial.print(",");
  Serial.print(centerValue);
  Serial.print(",");
  Serial.print(blowActive ? 1 : 0);
  Serial.print(",");
  Serial.println(inhaleActive ? 1 : 0);
}

void openOnScreenKeyboard() {

  Keyboard.press(KEY_LEFT_GUI);
  Keyboard.press(KEY_LEFT_CTRL);
  Keyboard.press('o');

  delay(80);

  Keyboard.releaseAll();
}

void openVoiceCommands() {

  Keyboard.press(KEY_LEFT_GUI);
  Keyboard.press('h');

  delay(80);

  Keyboard.releaseAll();
}

float axisSpeed(int raw) {

  if (raw == 0) return 0;

  float n = (float)raw / 512.0;

  if (n > 1) n = 1;
  if (n < -1) n = -1;

  float mag = fabs(n);
  mag = mag * mag;

  return (n < 0 ? -mag : mag) * maxSpeed;
}

void loop() {

  unsigned long now = millis();

  handleSerialCommands();

  unsigned long blowGap =
    keyboardMode ? keyboardGap : doubleGap;

  vertValue = analogRead(vertPin) - vertZero;
  horzValue = analogRead(horzPin) - horzZero;

  if (abs(vertValue) < deadZone)
    vertValue = 0;

  if (abs(horzValue) < deadZone)
    horzValue = 0;

  if ((digitalRead(selPin) == LOW) && (!mouseClickFlag)) {

    mouseClickFlag = 1;

    Mouse.press(MOUSE_LEFT);
  }

  else if ((digitalRead(selPin) == HIGH) && (mouseClickFlag)) {

    mouseClickFlag = 0;

    Mouse.release(MOUSE_LEFT);
  }

  int pressureValue = analogRead(pressurePin);

  filteredPressure =
    (filteredPressure * 0.70) +
    (pressureValue * 0.30);

  float diff = centerValue - filteredPressure;

  bool blowing =
    blowActive ?
    (diff > blowRelease) :
    (diff > blowThreshold);

  bool inhaling =
    inhaleActive ?
    (-diff > inhaleRelease) :
    (-diff > inhaleThreshold);

  if (blowing) {

    if (!blowActive) {

      blowActive = true;

      blowStartTime = now;

      longBlowActive = false;

      if (now - lastBlowEnd < blowGap)
        blowCount++;
      else
        blowCount = 1;

      sendEvent(
        blowCount >= 2 ?
        "BLOW_START_2" :
        "BLOW_START_1"
      );

      if (blowCount >= 2) {

        openOnScreenKeyboard();

        keyboardMode = !keyboardMode;

        blowCount = 0;

        lastBlowEnd = now;

        sendEvent(
          keyboardMode ?
          "KEYBOARD_ON" :
          "KEYBOARD_OFF"
        );
      }
    }

    else if (
      !longBlowActive &&
      (now - blowStartTime > longBlowTime)
    ) {

      longBlowActive = true;

      accX = accY = 0;

      blowCount = 0;

      sendEvent("SCROLL_MODE_ON");
    }
  }

  else {

    if (blowActive) {

      blowActive = false;

      lastBlowEnd = now;

      sendEvent("BLOW_END");

      if (longBlowActive) {

        longBlowActive = false;

        blowCount = 0;

        sendEvent("SCROLL_MODE_OFF");
      }
    }
  }

  if (
    !blowActive &&
    blowCount == 1 &&
    (now - lastBlowEnd > blowGap)
  ) {

    if (now - lastClickTime > clickInterval) {

      if (keyboardMode) {

        Mouse.click(MOUSE_LEFT);

        sendEvent("BLOW_KEY_PRESS");
      }

      else {

        Mouse.click(MOUSE_RIGHT);

        sendEvent("BLOW_RIGHT_CLICK");
      }

      lastClickTime = now;
    }

    blowCount = 0;
  }

  if (inhaling) {

    if (!inhaleActive) {

      inhaleActive = true;

      if (now - lastInhaleEnd < doubleGap)
        inhaleCount++;
      else
        inhaleCount = 1;

      sendEvent(
        inhaleCount >= 2 ?
        "INHALE_START_2" :
        "INHALE_START_1"
      );

      if (inhaleCount >= 2) {

        openVoiceCommands();

        inhaleCount = 0;

        lastInhaleEnd = now;

        sendEvent("VOICE_COMMANDS");
      }
    }
  }

  else {

    if (inhaleActive) {

      inhaleActive = false;

      lastInhaleEnd = now;

      sendEvent("INHALE_END");
    }
  }

  if (
    !inhaleActive &&
    inhaleCount == 1 &&
    (now - lastInhaleEnd > doubleGap)
  ) {

    if (keyboardMode) {

      inhaleCount = 0;

      sendEvent("INHALE_IGNORED_KB");
    }

    else if (now - lastClickTime > clickInterval) {

      Mouse.click(MOUSE_LEFT);

      lastClickTime = now;

      sendEvent("INHALE_LEFT_CLICK");

      inhaleCount = 0;
    }

    else {

      inhaleCount = 0;
    }
  }

  if (longBlowActive) {

    if (abs(vertValue) > scrollThreshold) {

      unsigned long iv =
        (abs(vertValue) > scrollFastThreshold)
        ? scrollFast
        : scrollSlow;

      if (now - lastScrollTime > iv) {

        int dir =
          (vertValue < 0) ?
          1 :
          -1;

        if (invertScroll)
          dir = -dir;

        Mouse.move(0, 0, dir);

        lastScrollTime = now;
      }
    }
  }

  else if (now - lastMoveTime >= moveInterval) {

    lastMoveTime = now;

    accX += axisSpeed(horzValue);
    accY += axisSpeed(vertValue);

    int dx = (int)accX;
    int dy = (int)accY;

    accX -= dx;
    accY -= dy;

    if (dx != 0 || dy != 0)
      Mouse.move(dx, dy);
  }

  if (now - lastTelemetryTime >= telemetryInterval) {

    lastTelemetryTime = now;

    sendTelemetry(now);
  }
}
