/* ================= BLYNK ================= */
#define BLYNK_TEMPLATE_ID "TMPL54GiblJiY"
#define BLYNK_TEMPLATE_NAME "wifi control car"
#define BLYNK_AUTH_TOKEN "2iouhAHqdPORU9FB-LuGwk3adU7wcOrZ"

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <TinyGPS++.h>
#include <math.h>

/* ================= WIFI ================= */
char ssid[] = "xxxxxxxxxxxx";// Replace with your WiFi name
char pass[] = "xxxxxxxxxxxx";//Replace with your WiFi password 

/* ================= MOTOR PINS ================= */
#define ENA 2
#define IN1 4
#define IN2 5
#define IN3 18
#define IN4 19
#define ENB 13

/* ================= OTHER PINS ================= */
#define BUZZER 23
#define TILT_THRESHOLD 60
#define MPU_ADDR 0x68

/* ================= UART ================= */
HardwareSerial gpsSerial(1);
HardwareSerial sim800l(2);

/* ================= OBJECTS ================= */
TinyGPSPlus gps;

/* ================= VARIABLES ================= */
String PHONE = "+91XXXXXXXXXX";//
bool accidentSent = false;
bool mpuConnected = false;
bool gpsDetected = false;

bool forward = 0;
bool backward = 0;
bool left = 0;
bool right = 0;
int Speed = 0;

unsigned long startTime;
unsigned long lastGPSPrint = 0;

/* ================= MPU INIT ================= */
void initMPU() {
  Wire.beginTransmission(MPU_ADDR);
  if (Wire.endTransmission() == 0) {
    mpuConnected = true;
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x6B);
    Wire.write(0x00);
    Wire.endTransmission(true);
    Serial.println("MPU Connected!");
  } else {
    mpuConnected = false;
    Serial.println("MPU NOT CONNECTED!");
  }
}

/* ================= GPS CHECK ================= */
void checkGPSConnection()
{
  Serial.println("Checking GPS Module...");
  unsigned long start = millis();

  while (millis() - start < 5000)
  {
    while (gpsSerial.available())
    {
      gps.encode(gpsSerial.read());
      gpsDetected = true;
    }
  }

  if (gpsDetected)
    Serial.println("GPS Module Detected!");
  else
    Serial.println("GPS NOT DETECTED!");
}

/* ================= SETUP ================= */
void setup()
{
  Serial.begin(115200);
  delay(1000);

  Serial.println("Starting System...");

  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");

  Blynk.config(BLYNK_AUTH_TOKEN);
  Blynk.connect();
  Serial.println("Blynk Connected!");

  Wire.begin(21,22);
  Wire.setClock(100000);
  initMPU();

  gpsSerial.begin(9600, SERIAL_8N1, 16, 17);
  checkGPSConnection();

  sim800l.begin(9600, SERIAL_8N1, 26, 27);
  delay(1000);
  sim800l.println("AT");
  delay(1000);
  sim800l.println("AT+CMGF=1");

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  ledcAttach(ENA, 1000, 8);
  ledcAttach(ENB, 1000, 8);

  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);

  startTime = millis();
  Serial.println("System Ready!");
}

/* ================= BLYNK CONTROLS ================= */
BLYNK_WRITE(V0) { Speed = constrain(param.asInt(), 0, 255); }
BLYNK_WRITE(V1) { forward = param.asInt(); }
BLYNK_WRITE(V2) { backward = param.asInt(); }
BLYNK_WRITE(V3) { left = param.asInt(); }
BLYNK_WRITE(V4) { right = param.asInt(); }

/* ================= LOOP ================= */
void loop()
{
  Blynk.run();
  WIFIcar();
  checkAccident();

  // Continuous GPS Fix Display
  while (gpsSerial.available())
    gps.encode(gpsSerial.read());

  if (gps.location.isValid() && millis() - lastGPSPrint > 3000)
  {
    Serial.println("GPS Fix Acquired!");
    Serial.print("Latitude: ");
    Serial.println(gps.location.lat(), 6);
    Serial.print("Longitude: ");
    Serial.println(gps.location.lng(), 6);
    Serial.println("-----------------------");
    lastGPSPrint = millis();
  }
}

/* ================= CAR CONTROL ================= */
void WIFIcar()
{
  if (forward) carForward();
  else if (backward) carBackward();
  else if (left) carLeft();
  else if (right) carRight();
  else carStop();
}

void carForward()
{
  ledcWrite(ENA, Speed);
  ledcWrite(ENB, Speed);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void carBackward()
{
  ledcWrite(ENA, Speed);
  ledcWrite(ENB, Speed);
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void carLeft()
{
  ledcWrite(ENA, Speed);
  ledcWrite(ENB, Speed);
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void carRight()
{
  ledcWrite(ENA, Speed);
  ledcWrite(ENB, Speed);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void carStop()
{
  ledcWrite(ENA, 0);
  ledcWrite(ENB, 0);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

/* ================= MPU TILT ================= */
float getTiltAngle()
{
  if (!mpuConnected) return 0;

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 6, true);

  if (Wire.available() < 6) return 0;

  int16_t ax = Wire.read() << 8 | Wire.read();
  int16_t ay = Wire.read() << 8 | Wire.read();
  int16_t az = Wire.read() << 8 | Wire.read();

  float axg = ax / 16384.0;
  float ayg = ay / 16384.0;
  float azg = az / 16384.0;

  float pitch = atan2(axg, sqrt(ayg * ayg + azg * azg)) * 57.3;
  float roll  = atan2(ayg, sqrt(axg * axg + azg * azg)) * 57.3;

  return max(abs(pitch), abs(roll));
}

/* ================= ACCIDENT DETECTION ================= */
void checkAccident()
{
  if (!mpuConnected) return;
  if (millis() - startTime < 5000) return;

  float tilt = getTiltAngle();
  Serial.print("Tilt: ");
  Serial.println(tilt);

  if (tilt > TILT_THRESHOLD && !accidentSent)
  {
    Serial.println("ACCIDENT DETECTED!");
    carStop();
    buzzerAlarm();

    if (gps.location.isValid())
    {
      float lat = gps.location.lat();
      float lng = gps.location.lng();

      Serial.println("Location Coordinates:");
      Serial.print("Latitude: ");
      Serial.println(lat, 6);
      Serial.print("Longitude: ");
      Serial.println(lng, 6);

      String msg = "ACCIDENT!\n";
      msg += "Lat: " + String(lat, 6) + "\n";
      msg += "Lng: " + String(lng, 6) + "\n";
      msg += "https://maps.google.com/?q=";
      msg += String(lat, 6);
      msg += ",";
      msg += String(lng, 6);

      sendSMS(msg);
    }
    else
    {
      Serial.println("GPS NOT FIXED!");
      sendSMS("ACCIDENT! GPS NOT FIXED");
    }

    callNumber();
    accidentSent = true;
  }
}

/* ================= GSM ================= */
void sendSMS(String message)
{
  sim800l.println("AT+CMGS=\"" + PHONE + "\"");
  delay(500);
  sim800l.print(message);
  delay(500);
  sim800l.write(26);
  delay(3000);
}

void callNumber()
{
  sim800l.println("ATD" + PHONE + ";");
  delay(5000);
}

/* ================= BUZZER ================= */
void buzzerAlarm()
{
  for (int i = 0; i < 10; i++)
  {
    digitalWrite(BUZZER, HIGH);
    delay(300);
    digitalWrite(BUZZER, LOW);
    delay(300);
  }
}
