/*
  PC Task Manager Dashboard
  ---------------------------
  Shows CPU / RAM / GPU usage all at once as three stacked bars - no
  pot interaction needed. A Python script on your PC reads live usage
  stats (GPU via AMD/Windows performance counters) and streams them
  over USB serial; this sketch just displays them. If any metric
  hits 99%+, crazy mode kicks in for that metric - a real overload
  warning, not just a gimmick.

  Pairs with: task_manager_sender.py (run on your computer)

  Hardware: Arduino UNO R4 Minima + SSD1306 128x64 (I2C)
  Libraries needed: Adafruit GFX Library, Adafruit SSD1306

  Serial protocol (from PC, once per ~1s):
    "<cpu>,<ram>,<gpu>\n"   e.g.  "42,67,55\n"
*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define SCREEN_ADDR   0x3C   // some boards use 0x3D

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const long BAUD = 115200;     // must match task_manager_sender.py

// Crazy-mode needle geometry (reused for the overload animation)
const int CX = 64, CY = 58; 
const int R_OUTER = 56, R_TICK_IN = 48, R_NEEDLE = 50;
const float ANGLE_START = 200.0;
const float ANGLE_END   = -20.0;

// --- Metrics fed in from the PC over serial ---
const int NUM_METRICS = 3;
float metrics[NUM_METRICS] = {0, 0, 0};         // last known CPU, RAM, GPU
const char* metricLabels[NUM_METRICS] = {"CPU", "RAM", "GPU"};
unsigned long lastSerialMs = 0;
const unsigned long SIGNAL_TIMEOUT_MS = 4000;   // "NO SIGNAL" if PC script stops

// --- Animation state, one per metric ---
float displayedVals[NUM_METRICS] = {0, 0, 0};

unsigned long lastFrame = 0;
const int FRAME_MS = 20;

void setup() {
  Serial.begin(BAUD);
  randomSeed(analogRead(A1));

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDR)) {
    while (1) delay(10);
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.display();
}

void loop() {
  readSerialMetrics();

  if (millis() - lastFrame < FRAME_MS) return;
  lastFrame = millis();

  bool noSignal = (millis() - lastSerialMs > SIGNAL_TIMEOUT_MS) && lastSerialMs != 0;
  bool neverConnected = (lastSerialMs == 0);

  if (noSignal || neverConnected) {
    drawNoSignal();
    return;
  }

  for (int i = 0; i < NUM_METRICS; i++) {
    displayedVals[i] += (metrics[i] - displayedVals[i]) * 0.3;
    if (fabs(metrics[i] - displayedVals[i]) < 1.0) displayedVals[i] = metrics[i];
  }

  drawDashboard();
}

// Reads lines like "42,67,55\n" from the PC and updates metrics[]
void readSerialMetrics() {
  static String buf = "";
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      int i1 = buf.indexOf(',');
      int i2 = buf.indexOf(',', i1 + 1);
      if (i1 > 0 && i2 > i1) {
        metrics[0] = buf.substring(0, i1).toFloat();
        metrics[1] = buf.substring(i1 + 1, i2).toFloat();
        metrics[2] = buf.substring(i2 + 1).toFloat();
        lastSerialMs = millis();
      }
      buf = "";
    } else if (c != '\r') {
      buf += c;
      if (buf.length() > 40) buf = ""; // guard against junk/garbage lines
    }
  }
}

void drawNoSignal() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(20, 24);
  display.print("Waiting for PC...");
  display.setCursor(10, 38);
  display.print("Run task_manager_sender.py");
  display.display();
}

// Draws all three metrics as stacked horizontal bars. If any metric is
// maxed (>=99), crazy mode takes over and shows that one instead.
void drawDashboard() {
  int maxIdx = 0;
  for (int i = 1; i < NUM_METRICS; i++) {
    if (displayedVals[i] > displayedVals[maxIdx]) maxIdx = i;
  }
  if (displayedVals[maxIdx] >= 99) {
    drawCrazyMode(displayedVals[maxIdx], metricLabels[maxIdx]);
    return;
  }

  display.clearDisplay();
  drawBarRow(2,  metricLabels[0], displayedVals[0]);
  drawBarRow(24, metricLabels[1], displayedVals[1]);
  drawBarRow(46, metricLabels[2], displayedVals[2]);
  display.display();
}

// One row: label + percentage on top, progress bar underneath
void drawBarRow(int y, const char* label, float value) {
  display.setTextSize(1);
  display.setCursor(0, y);
  display.print(label);

  String txt = String((int)value) + "%";
  int16_t x1, y1; uint16_t w, h;
  display.getTextBounds(txt, 0, 0, &x1, &y1, &w, &h);
  display.setCursor(SCREEN_WIDTH - w, y);
  display.print(txt);

  int barX = 0, barY = y + 10, barW = SCREEN_WIDTH, barH = 6;
  display.drawRect(barX, barY, barW, barH, SSD1306_WHITE);
  int fillW = constrain((int)(barW * (value / 100.0)), 0, barW);
  if (fillW > 2) {
    display.fillRect(barX + 1, barY + 1, fillW - 2, barH - 2, SSD1306_WHITE);
  }
}

void drawCrazyMode(float value, const char* label) {
  display.clearDisplay();

  int shakeX = random(-4, 5);
  int shakeY = random(-3, 4);
  int cx = CX + shakeX;
  int cy = CY + shakeY;
  float angleRange = ANGLE_START - ANGLE_END;

  for (int i = 0; i <= 100; i += 2) {
    float frac = i / 100.0;
    float ang = ANGLE_START - frac * angleRange;
    float rad = ang * PI / 180.0;
    int jitter = random(-3, 4);
    int x0 = cx + cos(rad) * (R_TICK_IN + jitter);
    int y0 = cy - sin(rad) * (R_TICK_IN + jitter);
    int x1 = cx + cos(rad) * (R_OUTER + jitter);
    int y1 = cy - sin(rad) * (R_OUTER + jitter);
    display.drawLine(x0, y0, x1, y1, SSD1306_WHITE);
  }

  float flailAng = ANGLE_END + random(-15, 16);
  float rad = flailAng * PI / 180.0;
  int nx = cx + cos(rad) * R_NEEDLE;
  int ny = cy - sin(rad) * R_NEEDLE;
  display.drawLine(cx, cy, nx, ny, SSD1306_WHITE);
  display.fillCircle(cx, cy, 3, SSD1306_WHITE);

  for (int i = 0; i < 40; i++) {
    display.drawPixel(random(0, SCREEN_WIDTH), random(0, SCREEN_HEIGHT), SSD1306_WHITE);
  }

  if ((millis() / 90) % 3 != 0) {
    display.fillRect(4, 22, 120, 20, SSD1306_BLACK);
    display.drawRect(4, 22, 120, 20, SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(10, 28);
    display.print(label);
    display.print(" MAXED!!");
  }

  display.invertDisplay((millis() / 70) % 5 == 0);
  display.display();
}
