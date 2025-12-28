/*
  Arduino GIGA R1 + Display Shield
  Touch robusto sin delay ni throttle
*/

#include "Arduino_GigaDisplay_GFX.h"
#include "Arduino_GigaDisplayTouch.h"
#include <math.h>

GigaDisplay_GFX display;
Arduino_GigaDisplayTouch touch;

#define SCREEN_W 480
#define SCREEN_H 800

#define DAC0_PIN A12
#define DAC1_PIN A13

// ===== TOUCH FIX =====
bool wasTouching = false;
unsigned long lastSeenTouchMs = 0;
const unsigned long releaseDebounceMs = 40; // anti-falso release (CLAVE)

// ===== TEXTO =====
uint16_t currentTextColor = 0x0000;
uint8_t currentTextSize = 2;

// ===== 3D =====
struct Point3D { float x,y,z; };
float cubeAngle = 0.0f;
float cubeX = SCREEN_W / 2.0f;
float cubeY = SCREEN_H / 2.0f;
float cubeSize = 100.0f;

// ---------- UTIL ----------
uint16_t parseColor(const String &c) {
  if (c=="BLACK") return 0x0000;
  if (c=="WHITE") return 0xFFFF;
  if (c=="RED") return 0xF800;
  if (c=="GREEN") return 0x07E0;
  if (c=="BLUE") return 0x001F;
  if (c=="YELLOW") return 0xFFE0;
  if (c=="CYAN") return 0x07FF;
  if (c=="MAGENTA") return 0xF81F;
  if (c=="ORANGE") return 0xFD20;
  if (c=="PURPLE") return 0x8010;
  if (c=="GRAY") return 0x8410;
  return 0xFFFF;
}

String readLine() {
  return Serial.readStringUntil('\n');
}

int idxComma(const String &s, int i) {
  return s.indexOf(',', i);
}

String sub(const String &s, int a, int b) {
  if (a < 0) a = 0;
  if (b < a) b = a;
  if (b > s.length()) b = s.length();
  return s.substring(a, b);
}

// ---------- 3D ----------
Point3D rotatePoint(Point3D p, float d) {
  float r = d * M_PI / 180.0;
  return {
    p.x * cos(r) - p.z * sin(r),
    p.y,
    p.x * sin(r) + p.z * cos(r)
  };
}

void drawCubeWire(uint16_t col) {
  display.fillScreen(0x0000);
  Point3D v[8];
  int i = 0;

  for (int x = -1; x <= 1; x += 2)
    for (int y = -1; y <= 1; y += 2)
      for (int z = -1; z <= 1; z += 2)
        v[i++] = rotatePoint({x * cubeSize, y * cubeSize, z * cubeSize}, cubeAngle);

  int p[8][2];
  for (i = 0; i < 8; i++) {
    p[i][0] = cubeX + v[i].x;
    p[i][1] = cubeY + v[i].y;
  }

  const int e[12][2] = {
    {0,1},{0,2},{0,4},{1,3},{1,5},
    {2,3},{2,6},{3,7},{4,5},
    {4,6},{5,7},{6,7}
  };

  for (i = 0; i < 12; i++)
    display.drawLine(
      p[e[i][0]][0], p[e[i][0]][1],
      p[e[i][1]][0], p[e[i][1]][1],
      col
    );
}

// ---------- SETUP ----------
void setup() {
  Serial.begin(2000000);
  display.begin();
  touch.begin();

  display.fillScreen(0xFFFF);
  display.setTextColor(currentTextColor);
  display.setTextSize(currentTextSize);
  display.setCursor(0, 0);
}

// ---------- LOOP ----------
void loop() {

  // ===== TOUCH ULTRA RÁPIDO Y ROBUSTO =====
  GDTpoint_t points[5];
  uint8_t contacts = touch.getTouchPoints(points);
  unsigned long now = millis();

  if (contacts > 0) {
    lastSeenTouchMs = now;
    wasTouching = true;

    Serial.print("{\"event\":\"touch\",\"x\":");
    Serial.print(points[0].x);
    Serial.print(",\"y\":");
    Serial.print(points[0].y);
    Serial.println("}");
  }
  else if (wasTouching && (now - lastSeenTouchMs) > releaseDebounceMs) {
    Serial.println("{\"event\":\"touch\",\"x\":0,\"y\":0}");
    wasTouching = false;
  }

  // ===== PARSER =====
  if (!Serial.available()) return;
  String cmd = readLine();

  if (cmd.startsWith("bg:"))
    display.fillScreen(parseColor(sub(cmd, 3, cmd.length())));

  else if (cmd.startsWith("clear:"))
    display.fillScreen(parseColor(sub(cmd, 6, cmd.length())));

  else if (cmd.startsWith("text:")) {
    int c1 = idxComma(cmd,5), c2 = idxComma(cmd,c1+1),
        c3 = idxComma(cmd,c2+1), c4 = idxComma(cmd,c3+1);
    display.setCursor(sub(cmd,c1+1,c2).toInt(),
                      sub(cmd,c2+1,c3).toInt());
    display.setTextColor(parseColor(sub(cmd,c3+1,c4)));
    display.setTextSize(sub(cmd,c4+1,cmd.length()).toInt());
    display.print(sub(cmd,5,c1));
  }

  else if (cmd.startsWith("rect:")) {
    int p1 = idxComma(cmd,5), p2 = idxComma(cmd,p1+1),
        p3 = idxComma(cmd,p2+1), p4 = idxComma(cmd,p3+1);
    display.fillRect(
      sub(cmd,5,p1).toInt(),
      sub(cmd,p1+1,p2).toInt(),
      sub(cmd,p2+1,p3).toInt(),
      sub(cmd,p3+1,p4).toInt(),
      parseColor(sub(cmd,p4+1,cmd.length()))
    );
  }

  else if (cmd.startsWith("dac:")) {
    int p1 = idxComma(cmd,4);
    analogWrite(
      sub(cmd,4,p1).toInt(),
      constrain(sub(cmd,p1+1,cmd.length()).toInt(), 0, 4095)
    );
  }

  else if (cmd.startsWith("rotatecube:")) {
    cubeAngle = sub(cmd,11,cmd.length()).toFloat();
    drawCubeWire(0xFFFF);
  }

  else if (cmd == "cube3d")
    drawCubeWire(0xFFFF);

  else if (cmd == "ping")
    Serial.println("{\"pong\":1}");
}
