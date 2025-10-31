#include <Arduino.h>

#include <Adafruit_NeoPixel.h>

#include "Button.h"
#include "DebugSerial.h"

#define SSD1306_128x64
// #define SSD1306_128x32
// #define SSD1306_96x16

#define RGBLED_PIN 16
#define BUTTON_PIN 29

Button button(BUTTON_PIN);
Adafruit_NeoPixel rgbled(1, RGBLED_PIN, NEO_GRB + NEO_KHZ800);

#if defined(SSD1306_128x64)
#include <Adafruit_SSD1306.h>
#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 64
#define DISPLAY_I2C_ADDRESS 0x3C
#define DISPLAY_WHITE SSD1306_WHITE
#define DISPLAY_BLACK SSD1306_BLACK
Adafruit_SSD1306 display(DISPLAY_WIDTH, DISPLAY_HEIGHT);
#endif

#if defined(SSD1306_128x32)
#include <Adafruit_SSD1306.h>
#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 32
#define DISPLAY_I2C_ADDRESS 0x3C
#define DISPLAY_WHITE SSD1306_WHITE
#define DISPLAY_BLACK SSD1306_BLACK
Adafruit_SSD1306 display(DISPLAY_WIDTH, DISPLAY_HEIGHT);
#endif

#if defined(SSD1306_96x16)
#include <Adafruit_SSD1306.h>
#define DISPLAY_WIDTH 96
#define DISPLAY_HEIGHT 16
#define DISPLAY_I2C_ADDRESS 0x3C
#define DISPLAY_WHITE SSD1306_WHITE
#define DISPLAY_BLACK SSD1306_BLACK
Adafruit_SSD1306 display(DISPLAY_WIDTH, DISPLAY_HEIGHT);
#endif

typedef enum {
  DISPLAY_PATTERN_GRAPHICS = 0,
  DISPLAY_PATTERN_TEXT,
  DISPLAY_PATTERN_LOGO,
} DisplayPattern;

typedef enum {
  RENDER_RESULT_DONE = 0,
  RENDER_RESULT_SWITCH_TO_NEXT,
} RenderResult;

typedef RenderResult (*RenderFunc)();

RenderResult renderGraphics();
RenderResult renderText();
RenderResult renderLogo();

RenderFunc renderFunctions[] = {
  renderGraphics,
  renderText,
  renderLogo,
};

void setup() {
  rgbled.begin();
  rgbled.setBrightness(127);
  blink(160, 160, 160, 200);

  Serial.begin(9600);
  while (!Serial && millis() < 1000) {}
  Serial.println();
  Serial.println("--");
  Serial.println("OLED-Tester (build " + timestamp() + ")");

  button.begin();
  if (!display.begin(SSD1306_SWITCHCAPVCC, DISPLAY_I2C_ADDRESS)) { DEBUG_SERIAL_PRINTLN("Failed to initialize display!"); }
  display.display();
  delay(1000);

  if (digitalRead(BUTTON_PIN) == LOW) {
    scan(Wire, display);
    while (digitalRead(BUTTON_PIN) == LOW) {}
  }
}

void loop() {
  static DisplayPattern displayPattern = DISPLAY_PATTERN_GRAPHICS;
  static bool needRender = true;

  button.update();

  if (button.isClicked()) {
    displayPattern = (DisplayPattern)((displayPattern + 1) % (sizeof(renderFunctions) / sizeof(renderFunctions[0])));
    needRender = true;
  }

  if (needRender) {
    RenderResult result = renderFunctions[displayPattern]();
    switch (result) {
      case RENDER_RESULT_DONE: needRender = false; break;
      case RENDER_RESULT_SWITCH_TO_NEXT:
        displayPattern = (DisplayPattern)((displayPattern + 1) % (sizeof(renderFunctions) / sizeof(renderFunctions[0])));
        needRender = true;
        break;
    }
  }

  delay(10);
}

void blink(uint8_t r, uint8_t g, uint8_t b, unsigned long durationMs) {
  rgbled.setPixelColor(0, r, g, b);
  rgbled.show();
  delay(durationMs);
  rgbled.setPixelColor(0, 0, 0, 0);
  rgbled.show();
  delay(durationMs);
}

String timestamp() {
  const char* m = "JanFebMarAprMayJunJulAugSepOctNovDec";
  char mon[4];
  int d, y, hh, mm, ss;
  sscanf(__DATE__, "%3s %d %d", mon, &d, &y);
  sscanf(__TIME__, "%d:%d:%d", &hh, &mm, &ss);
  int mo = (strstr(m, mon) - m) / 3 + 1;

  char buf[16];
  sprintf(buf, "%04d%02d%02d.%02d%02d%02d", y, mo, d, hh, mm, ss);
  return String(buf);
}

void scan(TwoWire& wire, Adafruit_SSD1306& display) {
  DEBUG_SERIAL_PRINTLN("SCANNING");
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("SCANNING");
  display.display();
  display.setCursor(0, 16);

  uint8_t deviceCount = 0;
  for (uint8_t address = 1; address < 127; address++) {
    wire.beginTransmission(address);
    uint8_t error = wire.endTransmission();
    if (error == 0) {
      if (address < 16) DEBUG_SERIAL_PRINT("0");
      DEBUG_SERIAL_PRINTHEX(address);
      DEBUG_SERIAL_PRINT(" ");
      if (deviceCount == 0) {
        display.clearDisplay();
        display.setCursor(0, 0);
      }
      display.printf("%X ", address);
      display.display();
      deviceCount++;
    }
    delay(10);
  }

  if (deviceCount == 0) {
    DEBUG_SERIAL_PRINTLN("NO DEVICES");
    display.clearDisplay();
    display.setCursor(0, 0);
    display.print("NO DEVICES");
    display.display();
  }
}

RenderResult renderGraphics() {
  display.clearDisplay();
  for (int i = 0; i < DISPLAY_WIDTH; i += 4) {
    button.update();
    if (button.isClicked()) return RENDER_RESULT_SWITCH_TO_NEXT;

    display.drawLine(i, 0, DISPLAY_WIDTH - i, DISPLAY_HEIGHT - 1, DISPLAY_WHITE);
    display.drawLine(0, i, DISPLAY_WIDTH - 1, DISPLAY_HEIGHT - i, DISPLAY_WHITE);
    display.drawLine(i, DISPLAY_HEIGHT - 1, DISPLAY_WIDTH - i, 0, DISPLAY_WHITE);
    display.drawLine(DISPLAY_WIDTH - 1, i, 0, DISPLAY_HEIGHT - i, DISPLAY_WHITE);
    display.display();
  }
  display.fillRect(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_WHITE);
  display.display();
  return RENDER_RESULT_DONE;
}

RenderResult renderText() {
  const String helloText = "Hello!";
  const String tickerText = ("OLED " + String(DISPLAY_WIDTH) + "x" + String(DISPLAY_HEIGHT) + " SSD1306");
  int16_t x1, y1;
  uint16_t w, h;

  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(DISPLAY_WHITE);
  display.getTextBounds(helloText, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((DISPLAY_WIDTH - w) / 2, (DISPLAY_HEIGHT - h) / 2);
  display.print(helloText);
  display.display();
  delay(100);

  display.clearDisplay();
  display.setTextSize(3);
  display.setTextWrap(false);
  display.getTextBounds(tickerText, 0, 0, &x1, &y1, &w, &h);
  for (int16_t tickerX = DISPLAY_WIDTH; tickerX >= -w; tickerX -= 2) {
    button.update();
    if (button.isClicked()) return RENDER_RESULT_SWITCH_TO_NEXT;

    display.fillRect(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_BLACK);
    display.setCursor(tickerX, (DISPLAY_HEIGHT - h) / 2);
    display.print(tickerText);
    display.display();
    delay(10);
  }
  delay(100);

  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(DISPLAY_WHITE);
  display.getTextBounds(helloText, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((DISPLAY_WIDTH - w) / 2, (DISPLAY_HEIGHT - h) / 2);
  display.print(helloText);
  display.display();
  return RENDER_RESULT_DONE;
}

RenderResult renderLogo() {
  display.begin(SSD1306_SWITCHCAPVCC, DISPLAY_I2C_ADDRESS);
  display.display();
  return RENDER_RESULT_DONE;
}
