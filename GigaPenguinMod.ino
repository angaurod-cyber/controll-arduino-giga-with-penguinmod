/*
  Arduino GIGA R1 + Display Shield
  Firmware extendido: pantalla, texto, figuras, touch, IO, tono, DAC, 3D y matemáticas
  Silencioso al iniciar (solo emite eventos/JSON en respuesta a acciones)
*/

#include "Arduino_GigaDisplay_GFX.h"
#include "Arduino_GigaDisplayTouch.h"
#include <math.h>

GigaDisplay_GFX display;
Arduino_GigaDisplayTouch touch;

#define SCREEN_X 480
#define SCREEN_Y 800

// Alias de pines DAC válidos en GIGA (canales reales)
#define DAC0_PIN DAC  // primer DAC
#define DAC1_PIN A13  // segundo DAC

// Resoluciones para ADC/DAC (12 bits)
const int ADC_RES = 12;
const int DAC_RES = 12;

// Throttle para eventos touch
unsigned long lastTouchMs = 0;
const unsigned long touchIntervalMs = 40;

// Estado de texto actual
uint16_t currentTextColor = 0x0000; // negro
uint8_t currentTextSize = 2;

// Estado del cubo 3D
struct Point3D { float x,y,z; };
float cubeAngle = 0.0f;
float cubeX = SCREEN_X / 2.0f;
float cubeY = SCREEN_Y / 2.0f;
float cubeSize = 100.0f;

// -------------------- Utilidades --------------------
uint16_t parseColor(const String &c) {
  if (c == "BLACK")   return 0x0000;
  if (c == "WHITE")   return 0xFFFF;
  if (c == "RED")     return 0xF800;
  if (c == "GREEN")   return 0x07E0;
  if (c == "BLUE")    return 0x001F;
  if (c == "YELLOW")  return 0xFFE0;
  if (c == "CYAN")    return 0x07FF;
  if (c == "MAGENTA") return 0xF81F;
  if (c == "ORANGE")  return 0xFD20;
  if (c == "PURPLE")  return 0x8010;
  if (c == "GRAY")    return 0x8410;
  return 0xFFFF;
}

String readLine() { return Serial.readStringUntil('\n'); }

int idxComma(const String &s, int startIdx) { return s.indexOf(',', startIdx); }

// Cortes seguros de substring
String sub(const String &s, int a, int b) {
  int L = s.length();
  if (a < 0) a = 0;
  if (b < a) b = a;
  if (b > L) b = L;
  return s.substring(a, b);
}

// -------------------- 3D --------------------
Point3D rotatePoint(Point3D p, float angleDeg) {
  float rad = angleDeg * M_PI / 180.0;
  float cosA = cos(rad);
  float sinA = sin(rad);
  return { p.x*cosA - p.z*sinA, p.y, p.x*sinA + p.z*cosA };
}

void drawCubeWire(uint16_t color = 0xFFFF) {
  // Limpia para que el cubo se vea claro
  display.fillScreen(0x0000);

  Point3D vertices[8];
  int idx = 0;
  for (int dx=-1; dx<=1; dx+=2) {
    for (int dy=-1; dy<=1; dy+=2) {
      for (int dz=-1; dz<=1; dz+=2) {
        vertices[idx++] = rotatePoint({dx*cubeSize, dy*cubeSize, dz*cubeSize}, cubeAngle);
      }
    }
  }

  // Proyección ortográfica simple al plano XY
  int proj[8][2];
  for (int i=0;i<8;i++) {
    proj[i][0] = (int)(cubeX + vertices[i].x);
    proj[i][1] = (int)(cubeY + vertices[i].y);
  }

  // Aristas del cubo
  const int edges[12][2] = {
    {0,1},{0,2},{0,4},{1,3},{1,5},{2,3},{2,6},{3,7},
    {4,5},{4,6},{5,7},{6,7}
  };
  for (int e=0;e<12;e++) {
    int a = edges[e][0];
    int b = edges[e][1];
    display.drawLine(proj[a][0], proj[a][1], proj[b][0], proj[b][1], color);
  }
}

// -------------------- Setup --------------------
void setup() {
  Serial.begin(2000000);

  display.begin();
  touch.begin();

  analogReadResolution(ADC_RES);
  analogWriteResolution(DAC_RES);

  // Pantalla clara por defecto
  display.fillScreen(0xFFFF);
  display.setTextColor(currentTextColor);
  display.setTextSize(currentTextSize);
  display.setCursor(0, 0);
}

// -------------------- Loop --------------------
void loop() {
  // Eventos de touch con throttling
  GDTpoint_t points[5];
  uint8_t contacts = touch.getTouchPoints(points);
  unsigned long now = millis();
  if (contacts > 0 && (now - lastTouchMs) >= touchIntervalMs) {
    Serial.print("{\"event\":\"touch\",\"x\":");
    Serial.print(points[0].x);
    Serial.print(",\"y\":");
    Serial.print(points[0].y);
    Serial.println("}");
    lastTouchMs = now;
  }

  // Comandos por Serial
  if (Serial.available()) {
    String cmd = readLine();

    // ---------- Fondo / limpieza ----------
    // bg:COLOR
    if (cmd.startsWith("bg:")) {
      display.fillScreen(parseColor(sub(cmd, 3, cmd.length())));
    }
    // clear:COLOR
    else if (cmd.startsWith("clear:")) {
      uint16_t c = parseColor(sub(cmd, 6, cmd.length()));
      display.fillScreen(c);
    }

    // ---------- Rotación de pantalla ----------
    // rotation:DEG (0, 90, 180, 270)
    else if (cmd.startsWith("rotation:")) {
      int deg = sub(cmd, 9, cmd.length()).toInt();
      // Mapea a índices 0..3
      uint8_t rotIndex = 0;
      if (deg == 0) rotIndex = 0;
      else if (deg == 90) rotIndex = 1;
      else if (deg == 180) rotIndex = 2;
      else if (deg == 270) rotIndex = 3;
      display.setRotation(rotIndex);
    }

    // ---------- Texto avanzado ----------
    // text:STRING,x,y,COLOR,SIZE
    else if (cmd.startsWith("text:")) {
      int c1 = idxComma(cmd, 5);
      int c2 = idxComma(cmd, c1 + 1);
      int c3 = idxComma(cmd, c2 + 1);
      int c4 = idxComma(cmd, c3 + 1);
      String txt = sub(cmd, 5, c1);
      int x      = sub(cmd, c1 + 1, c2).toInt();
      int y      = sub(cmd, c2 + 1, c3).toInt();
      String col = (c4 > 0) ? sub(cmd, c3 + 1, c4) : sub(cmd, c3 + 1, cmd.length());
      int size   = (c4 > 0) ? sub(cmd, c4 + 1, cmd.length()).toInt() : currentTextSize;

      display.setCursor(x, y);
      display.setTextColor(parseColor(col));
      display.setTextSize(size);
      display.print(txt);

      // Restaura estado actual
      display.setTextColor(currentTextColor);
      display.setTextSize(currentTextSize);
    }

    // textcolor:COLOR
    else if (cmd.startsWith("textcolor:")) {
      currentTextColor = parseColor(sub(cmd, 10, cmd.length()));
      display.setTextColor(currentTextColor);
    }

    // textsize:N (1..8 aprox)
    else if (cmd.startsWith("textsize:")) {
      int sz = sub(cmd, 9, cmd.length()).toInt();
      if (sz < 1) sz = 1;
      if (sz > 8) sz = 8;
      currentTextSize = sz;
      display.setTextSize(currentTextSize);
    }

    // cursor:x,y
    else if (cmd.startsWith("cursor:")) {
      int p1 = idxComma(cmd, 7);
      int x = sub(cmd, 7, p1).toInt();
      int y = sub(cmd, p1 + 1, cmd.length()).toInt();
      display.setCursor(x, y);
    }

    // ---------- Figuras y líneas ----------
    // line:x1,y1,x2,y2,COLOR
    else if (cmd.startsWith("line:")) {
      int p1 = idxComma(cmd, 5);
      int p2 = idxComma(cmd, p1 + 1);
      int p3 = idxComma(cmd, p2 + 1);
      int p4 = idxComma(cmd, p3 + 1);
      int x1 = sub(cmd, 5, p1).toInt();
      int y1 = sub(cmd, p1 + 1, p2).toInt();
      int x2 = sub(cmd, p2 + 1, p3).toInt();
      int y2 = sub(cmd, p3 + 1, p4).toInt();
      String col = sub(cmd, p4 + 1, cmd.length());
      display.drawLine(x1, y1, x2, y2, parseColor(col));
    }

    // rect:x,y,w,h,COLOR (relleno)
    else if (cmd.startsWith("rect:")) {
    int p1 = idxComma(cmd, 5);          // coma después de x
    int p2 = idxComma(cmd, p1 + 1);     // coma después de y
    int p3 = idxComma(cmd, p2 + 1);     // coma después de w
    int p4 = idxComma(cmd, p3 + 1);     // coma después de h

    int x = sub(cmd, 5, p1).toInt();            // primer valor → X
    int y = sub(cmd, p1 + 1, p2).toInt();       // segundo valor → Y
    int w = sub(cmd, p2 + 1, p3).toInt();       // tercero → ancho
    int h = sub(cmd, p3 + 1, p4).toInt();       // cuarto → alto
    String col = sub(cmd, p4 + 1, cmd.length()); // quinto → color

    display.fillRect(x, y, w, h, parseColor(col));
}

    // recto:x,y,w,h,COLOR (borde)
    else if (cmd.startsWith("recto:")) {
      int p1 = idxComma(cmd, 6);
      int p2 = idxComma(cmd, p1 + 1);
      int p3 = idxComma(cmd, p2 + 1);
      int p4 = idxComma(cmd, p3 + 1);
      int x = sub(cmd, 6, p1).toInt();
      int y = sub(cmd, p1 + 1, p2).toInt();
      int w = sub(cmd, p2 + 1, p3).toInt();
      int h = sub(cmd, p3 + 1, p4).toInt();
      String col = sub(cmd, p4 + 1, cmd.length());
      uint16_t c = parseColor(col);
      display.drawLine(x, y, x+w, y, c);
      display.drawLine(x+w, y, x+w, y+h, c);
      display.drawLine(x+w, y+h, x, y+h, c);
      display.drawLine(x, y+h, x, y, c);
    }

    // circle:x,y,r,COLOR (relleno)
    else if (cmd.startsWith("circle:")) {
      int p1 = idxComma(cmd, 7);
      int p2 = idxComma(cmd, p1 + 1);
      int p3 = idxComma(cmd, p2 + 1);
      int x = sub(cmd, 7, p1).toInt();
      int y = sub(cmd, p1 + 1, p2).toInt();
      int r = sub(cmd, p2 + 1, p3).toInt();
      String col = sub(cmd, p3 + 1, cmd.length());
      display.fillCircle(x, y, r, parseColor(col));
    }

    // circleo:x,y,r,COLOR (borde)
    else if (cmd.startsWith("circleo:")) {
      int p1 = idxComma(cmd, 8);
      int p2 = idxComma(cmd, p1 + 1);
      int p3 = idxComma(cmd, p2 + 1);
      int x = sub(cmd, 8, p1).toInt();
      int y = sub(cmd, p1 + 1, p2).toInt();
      int r = sub(cmd, p2 + 1, p3).toInt();
      String col = sub(cmd, p3 + 1, cmd.length());
      display.drawCircle(x, y, r, parseColor(col));
    }

    // tri:x1,y1,x2,y2,x3,y3,COLOR (borde)
    else if (cmd.startsWith("tri:")) {
      int p1 = idxComma(cmd, 4);
      int p2 = idxComma(cmd, p1 + 1);
      int p3 = idxComma(cmd, p2 + 1);
      int p4 = idxComma(cmd, p3 + 1);
      int p5 = idxComma(cmd, p4 + 1);
      int p6 = idxComma(cmd, p5 + 1);

      int x1 = sub(cmd, 4, p1).toInt();
      int y1 = sub(cmd, p1 + 1, p2).toInt();
      int x2 = sub(cmd, p2 + 1, p3).toInt();
      int y2 = sub(cmd, p3 + 1, p4).toInt();
      int x3 = sub(cmd, p4 + 1, p5).toInt();
      int y3 = sub(cmd, p5 + 1, p6).toInt();
      String col = sub(cmd, p6 + 1, cmd.length());
      uint16_t c = parseColor(col);

      display.drawLine(x1, y1, x2, y2, c);
      display.drawLine(x2, y2, x3, y3, c);
      display.drawLine(x3, y3, x1, y1, c);
    }

    // ---------- Digital I/O ----------
    // pin:PIN,HIGH | pin:PIN,LOW | pin:PIN,READ
    else if (cmd.startsWith("pin:")) {
      int p1 = idxComma(cmd, 4);
      int pin = sub(cmd, 4, p1).toInt();
      String action = sub(cmd, p1 + 1, cmd.length());
      if (action == "HIGH") { pinMode(pin, OUTPUT); digitalWrite(pin, HIGH); }
      else if (action == "LOW") { pinMode(pin, OUTPUT); digitalWrite(pin, LOW); }
      else if (action == "READ") { pinMode(pin, INPUT);
        int val = digitalRead(pin);
        Serial.print("{\"pin\":"); Serial.print(pin);
        Serial.print(",\"val\":"); Serial.print(val); Serial.println("}");
      }
    }

    // ---------- Analog Read ----------
    // analog:PIN
    else if (cmd.startsWith("analog:")) {
      int pin = sub(cmd, 7, cmd.length()).toInt();
      int val = analogRead(pin);
      Serial.print("{\"analog\":"); Serial.print(pin);
      Serial.print(",\"val\":"); Serial.print(val); Serial.println("}");
    }

    // ---------- Tone ----------
    // tone:PIN,FREQ,DUR_MS
    else if (cmd.startsWith("tone:")) {
      int p1 = idxComma(cmd, 5);
      int p2 = idxComma(cmd, p1 + 1);
      int pin  = sub(cmd, 5, p1).toInt();
      int freq = sub(cmd, p1 + 1, p2).toInt();
      int dur  = sub(cmd, p2 + 1, cmd.length()).toInt();
      tone(pin, freq, dur);
    }

    // notone:PIN
    else if (cmd.startsWith("notone:")) {
      int pin = sub(cmd, 7, cmd.length()).toInt();
      noTone(pin);
    }

    // ---------- DAC ----------
    // dac:PIN,VALUE   (VALUE 0..4095)
    else if (cmd.startsWith("dac:")) {
      int p1 = idxComma(cmd, 4);
      int pin = sub(cmd, 4, p1).toInt();
      int value = sub(cmd, p1 + 1, cmd.length()).toInt();
      if (value < 0) value = 0;
      if (value > 4095) value = 4095;
      analogWrite(pin, value);
      if (pin == DAC0_PIN) { Serial.print("{\"dac0\":"); Serial.print(value); Serial.println("}"); }
      if (pin == DAC1_PIN) { Serial.print("{\"dac1\":"); Serial.print(value); Serial.println("}"); }
    }

    // ---------- 3D cubo ----------
    // cube3d            (dibuja cubo con estado actual)
    else if (cmd.startsWith("cube3d")) {
      drawCubeWire(0xFFFF);
    }
    // rotatecube:DEG    (rota cubo)
    else if (cmd.startsWith("rotatecube:")) {
      cubeAngle = sub(cmd, 11, cmd.length()).toFloat();
      drawCubeWire(0xFFFF);
    }
    // movecubex:DX      (desplaza cubo en X)
    else if (cmd.startsWith("movecubex:")) {
      cubeX += sub(cmd, 10, cmd.length()).toInt();
      drawCubeWire(0xFFFF);
    }
    // movecubey:DY      (desplaza cubo en Y)
    else if (cmd.startsWith("movecubey:")) {
      cubeY += sub(cmd, 10, cmd.length()).toInt();
      drawCubeWire(0xFFFF);
    }
    // cubesize:S        (cambia tamaño del cubo)
    else if (cmd.startsWith("cubesize:")) {
      cubeSize = sub(cmd, 9, cmd.length()).toFloat();
      if (cubeSize < 10) cubeSize = 10;
      drawCubeWire(0xFFFF);
    }

    // ---------- Matemáticas ----------
    // math:expr (soporta + - * / de dos operandos)
    else if (cmd.startsWith("math:")) {
      String expr = sub(cmd, 5, cmd.length());
      double result = NAN;

      if (expr.indexOf('+') > 0) {
        int pos = expr.indexOf('+');
        double a = sub(expr, 0, pos).toDouble();
        double b = sub(expr, pos+1, expr.length()).toDouble();
        result = a + b;
      }
      else if (expr.indexOf('-') > 0) {
        int pos = expr.indexOf('-');
        double a = sub(expr, 0, pos).toDouble();
        double b = sub(expr, pos+1, expr.length()).toDouble();
        result = a - b;
      }
      else if (expr.indexOf('*') > 0) {
        int pos = expr.indexOf('*');
        double a = sub(expr, 0, pos).toDouble();
        double b = sub(expr, pos+1, expr.length()).toDouble();
        result = a * b;
      }
      else if (expr.indexOf('/') > 0) {
        int pos = expr.indexOf('/');
        double a = sub(expr, 0, pos).toDouble();
        double b = sub(expr, pos+1, expr.length()).toDouble();
        if (b == 0) {
          Serial.println("{\"math_error\":\"division_by_zero\"}");
          // No usamos continue; fuera de bucles.
        } else {
          result = a / b;
        }
      }

      if (isnan(result)) {
        Serial.println("{\"math_error\":\"unsupported_expression\"}");
      } else {
        Serial.print("{\"math\":\""); Serial.print(expr);
        Serial.print("\",\"result\":"); Serial.print(result); Serial.println("}");
      }
    }

    // ---------- Eco simple para debug ----------
    // ping -> responde pong
    else if (cmd == "ping") {
      Serial.println("{\"pong\":1}");
    }
  }
}