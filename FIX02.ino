/*
  Arduino GIGA R1 + Display Shield
  Firmware FULL compatible con Scratch / WebSerial
  Touch robusto + parser seguro
*/

#include "Arduino_GigaDisplay_GFX.h"
#include "Arduino_GigaDisplayTouch.h"
#include <math.h>

GigaDisplay_GFX display;
Arduino_GigaDisplayTouch touch;

#define SCREEN_X 480
#define SCREEN_Y 800

#define DAC0_PIN A12
#define DAC1_PIN A13

const int ADC_RES = 12;
const int DAC_RES = 12;

// ===== TOUCH FIX =====
bool wasTouching = false;
unsigned long lastSeenTouchMs = 0;
const unsigned long releaseDebounceMs = 35; // rápido y estable

// ===== TEXTO =====
uint16_t currentTextColor = 0x0000;
uint8_t currentTextSize = 2;

// ===== 3D =====
struct Point3D { float x,y,z; };
float cubeAngle = 0;
float cubeX = SCREEN_X/2;
float cubeY = SCREEN_Y/2;
float cubeSize = 120;

// ================== UTILIDADES ==================
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

String readLine() { return Serial.readStringUntil('\n'); }
int idxComma(const String &s, int i) { return s.indexOf(',', i); }

String sub(const String &s,int a,int b){
  if(a<0)a=0;
  if(b<a)b=a;
  if(b>(int)s.length())b=s.length();
  return s.substring(a,b);
}

// ================== 3D ==================
Point3D rotatePoint(Point3D p,float a){
  float r=a*PI/180.0;
  return {p.x*cos(r)-p.z*sin(r),p.y,p.x*sin(r)+p.z*cos(r)};
}

void drawCube(){
  display.fillScreen(0x0000);
  Point3D v[8];
  int i=0;
  for(int x=-1;x<=1;x+=2)
    for(int y=-1;y<=1;y+=2)
      for(int z=-1;z<=1;z+=2)
        v[i++]=rotatePoint({x*cubeSize,y*cubeSize,z*cubeSize},cubeAngle);

  int p[8][2];
  for(i=0;i<8;i++){
    p[i][0]=cubeX+v[i].x;
    p[i][1]=cubeY+v[i].y;
  }

  int e[12][2]={{0,1},{0,2},{0,4},{1,3},{1,5},{2,3},{2,6},{3,7},{4,5},{4,6},{5,7},{6,7}};
  for(i=0;i<12;i++)
    display.drawLine(p[e[i][0]][0],p[e[i][0]][1],p[e[i][1]][0],p[e[i][1]][1],0xFFFF);
}

// ================== SETUP ==================
void setup(){
  Serial.begin(2000000);
  display.begin();
  touch.begin();
  analogReadResolution(ADC_RES);
  analogWriteResolution(DAC_RES);
  display.fillScreen(0xFFFF);
}

// ================== LOOP ==================
void loop(){

// ===== TOUCH ROBUSTO =====
  GDTpoint_t pts[5];
  uint8_t c = touch.getTouchPoints(pts);
  unsigned long now = millis();

  if(c>0){
    lastSeenTouchMs = now;
    wasTouching = true;
    Serial.print("{\"event\":\"touch\",\"x\":");
    Serial.print(pts[0].x);
    Serial.print(",\"y\":");
    Serial.print(pts[0].y);
    Serial.println("}");
  }
  else if(wasTouching && (now-lastSeenTouchMs)>releaseDebounceMs){
    Serial.println("{\"event\":\"touch\",\"x\":0,\"y\":0}");
    wasTouching=false;
  }

// ===== SERIAL COMMANDS =====
  if(!Serial.available()) return;
  String cmd = readLine();

  // Pantalla
  if(cmd.startsWith("bg:")) display.fillScreen(parseColor(sub(cmd,3,cmd.length())));
  else if(cmd.startsWith("clear:")) display.fillScreen(parseColor(sub(cmd,6,cmd.length())));
  else if(cmd.startsWith("rotation:")){
    int d=sub(cmd,9,cmd.length()).toInt();
    display.setRotation((d/90)%4);
  }

  // Texto
  else if(cmd.startsWith("text:")){
    int c1=idxComma(cmd,5),c2=idxComma(cmd,c1+1),c3=idxComma(cmd,c2+1),c4=idxComma(cmd,c3+1);
    if(c1<0||c2<0||c3<0||c4<0)return;
    display.setCursor(sub(cmd,c1+1,c2).toInt(),sub(cmd,c2+1,c3).toInt());
    display.setTextColor(parseColor(sub(cmd,c3+1,c4)));
    display.setTextSize(sub(cmd,c4+1,cmd.length()).toInt());
    display.print(sub(cmd,5,c1));
  }
  else if(cmd.startsWith("textcolor:")){
    currentTextColor=parseColor(sub(cmd,10,cmd.length()));
    display.setTextColor(currentTextColor);
  }
  else if(cmd.startsWith("textsize:")){
    currentTextSize=sub(cmd,9,cmd.length()).toInt();
    display.setTextSize(currentTextSize);
  }
  else if(cmd.startsWith("cursor:")){
    int p=idxComma(cmd,7); if(p<0)return;
    display.setCursor(sub(cmd,7,p).toInt(),sub(cmd,p+1,cmd.length()).toInt());
  }

  // Figuras
  else if(cmd.startsWith("line:")){
    int p1=idxComma(cmd,5),p2=idxComma(cmd,p1+1),p3=idxComma(cmd,p2+1),p4=idxComma(cmd,p3+1);
    if(p1<0||p2<0||p3<0||p4<0)return;
    display.drawLine(sub(cmd,5,p1).toInt(),sub(cmd,p1+1,p2).toInt(),
                     sub(cmd,p2+1,p3).toInt(),sub(cmd,p3+1,p4).toInt(),
                     parseColor(sub(cmd,p4+1,cmd.length())));
  }
  else if(cmd.startsWith("rect:")){
    int p1=idxComma(cmd,5),p2=idxComma(cmd,p1+1),p3=idxComma(cmd,p2+1),p4=idxComma(cmd,p3+1);
    if(p1<0||p2<0||p3<0||p4<0)return;
    display.fillRect(sub(cmd,5,p1).toInt(),sub(cmd,p1+1,p2).toInt(),
                     sub(cmd,p2+1,p3).toInt(),sub(cmd,p3+1,p4).toInt(),
                     parseColor(sub(cmd,p4+1,cmd.length())));
  }
  else if(cmd.startsWith("circle:")){
    int p1=idxComma(cmd,7),p2=idxComma(cmd,p1+1),p3=idxComma(cmd,p2+1);
    if(p1<0||p2<0||p3<0)return;
    display.fillCircle(sub(cmd,7,p1).toInt(),sub(cmd,p1+1,p2).toInt(),
                       sub(cmd,p2+1,p3).toInt(),
                       parseColor(sub(cmd,p3+1,cmd.length())));
  }
  else if(cmd.startsWith("tri:")){
    int p1=idxComma(cmd,4),p2=idxComma(cmd,p1+1),p3=idxComma(cmd,p2+1),
        p4=idxComma(cmd,p3+1),p5=idxComma(cmd,p4+1),p6=idxComma(cmd,p5+1);
    if(p1<0||p2<0||p3<0||p4<0||p5<0||p6<0)return;
    uint16_t c=parseColor(sub(cmd,p6+1,cmd.length()));
    display.drawLine(sub(cmd,4,p1).toInt(),sub(cmd,p1+1,p2).toInt(),
                     sub(cmd,p2+1,p3).toInt(),sub(cmd,p3+1,p4).toInt(),c);
    display.drawLine(sub(cmd,p2+1,p3).toInt(),sub(cmd,p3+1,p4).toInt(),
                     sub(cmd,p4+1,p5).toInt(),sub(cmd,p5+1,p6).toInt(),c);
    display.drawLine(sub(cmd,p4+1,p5).toInt(),sub(cmd,p5+1,p6).toInt(),
                     sub(cmd,4,p1).toInt(),sub(cmd,p1+1,p2).toInt(),c);
  }

  // IO
  else if(cmd.startsWith("pin:")){
    int p=idxComma(cmd,4); if(p<0)return;
    int pin=sub(cmd,4,p).toInt();
    String a=sub(cmd,p+1,cmd.length());
    if(a=="HIGH"){pinMode(pin,OUTPUT);digitalWrite(pin,HIGH);}
    else if(a=="LOW"){pinMode(pin,OUTPUT);digitalWrite(pin,LOW);}
    else if(a=="READ"){pinMode(pin,INPUT);Serial.print("{\"pin\":");Serial.print(pin);Serial.print(",\"val\":");Serial.print(digitalRead(pin));Serial.println("}");}
  }
  else if(cmd.startsWith("analog:")){
    int pin=sub(cmd,7,cmd.length()).toInt();
    Serial.print("{\"analog\":");Serial.print(pin);Serial.print(",\"val\":");Serial.print(analogRead(pin));Serial.println("}");
  }

  // Audio
  else if(cmd.startsWith("tone:")){
    int p1=idxComma(cmd,5),p2=idxComma(cmd,p1+1);
    if(p1<0||p2<0)return;
    tone(sub(cmd,5,p1).toInt(),sub(cmd,p1+1,p2).toInt(),sub(cmd,p2+1,cmd.length()).toInt());
  }
  else if(cmd.startsWith("notone:")) noTone(sub(cmd,7,cmd.length()).toInt());

  // DAC
  else if(cmd.startsWith("dac:")){
    int p=idxComma(cmd,4); if(p<0)return;
    int pin=sub(cmd,4,p).toInt();
    int v=sub(cmd,p+1,cmd.length()).toInt();
    analogWrite(pin,v);
    Serial.print(pin==DAC0_PIN?"{\"dac0\":":"{\"dac1\":");Serial.print(v);Serial.println("}");
  }

  // 3D
  else if(cmd=="cube3d") drawCube();
  else if(cmd.startsWith("rotatecube:")){cubeAngle=sub(cmd,11,cmd.length()).toFloat();drawCube();}
  else if(cmd.startsWith("movecubex:")){cubeX+=sub(cmd,10,cmd.length()).toInt();drawCube();}
  else if(cmd.startsWith("movecubey:")){cubeY+=sub(cmd,10,cmd.length()).toInt();drawCube();}
  else if(cmd.startsWith("cubesize:")){cubeSize=sub(cmd,9,cmd.length()).toFloat();drawCube();}

  // Math
  else if(cmd.startsWith("math:")){
    String e=sub(cmd,5,cmd.length());
    double r=NAN;
    if(e.indexOf('+')>0){int p=e.indexOf('+');r=e.substring(0,p).toDouble()+e.substring(p+1).toDouble();}
    else if(e.indexOf('-')>0){int p=e.indexOf('-');r=e.substring(0,p).toDouble()-e.substring(p+1).toDouble();}
    else if(e.indexOf('*')>0){int p=e.indexOf('*');r=e.substring(0,p).toDouble()*e.substring(p+1).toDouble();}
    else if(e.indexOf('/')>0){int p=e.indexOf('/');double b=e.substring(p+1).toDouble();if(b!=0)r=e.substring(0,p).toDouble()/b;}
    if(!isnan(r)){Serial.print("{\"math\":\"");Serial.print(e);Serial.print("\",\"result\":");Serial.print(r);Serial.println("}");}
  }

  else if(cmd=="ping") Serial.println("{\"pong\":1}");
}



//sigue siendo compatible con arduino ide aunque lo hice con visual studio code esta sin errores soluciono el circulo linea y triangulo