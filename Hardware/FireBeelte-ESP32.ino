// FireBeetle ESP32 V3 + MPU6050 (I2C) + 5 Flex Sensors (A0..A4)
// Output one CSV line: roll,pitch,yaw,ax,ay,az,f0,f1,f2,f3,f4
// Press ENTER in Serial Monitor to pause/resume streaming.

#include <Wire.h>
#include <math.h>
#include <WiFi.h>
#include <PubSubClient.h>

// ---- MPU6050 ----
static const uint8_t MPU_ADDR = 0x68;
static const uint8_t REG_PWR_MGMT_1   = 0x6B;
static const uint8_t REG_ACCEL_XOUT_H = 0x3B;
static const uint8_t REG_GYRO_CONFIG  = 0x1B;
static const uint8_t REG_ACCEL_CONFIG = 0x1C;

static const float ACCEL_SENS = 16384.0f;  // LSB/g at ±2g
static const float GYRO_SENS  = 131.0f;    // LSB/(deg/s) at ±250 dps

// ---- Filters / tuning ----
static const float CF_ALPHA = 0.98f;   // complementary filter gyro trust
static const float ACC_LP   = 0.15f;   // accel EMA low-pass (0..1)
static const float FLEX_ALPHA = 0.25f; // flex EMA smoothing (0..1), larger = snappier
static const uint32_t CALIB_MS = 2000; // keep still during this

// ---- Pins ----
const int FLEX_PINS[5] = {A0, A1, A2, A3, A4};

// ---- State ----
float gyroBias[3] = {0,0,0};
float ax_f=0, ay_f=0, az_f=0;          // accel LP
float roll=0, pitch=0, yaw=0;          // deg
bool  paused = false;

int   flexBase[5] = {0,0,0,0,0};       // baseline (relaxed hand)
float flexEMA[5]  = {0,0,0,0,0};       // smoothed centered values

// median-of-3 buffers for flex (spike suppression)
int   fHist[5][3] = {0};               // circular history
uint8_t fIdx = 0;

uint32_t lastMicros = 0;

// ---- Wi-Fi and MQTT credentials ----
const char* ssid     = "WiFi_Name";
const char* password = "WiFi_Password";
const char* mqttServer = "10.21.237.195";  // Replace with your server's IP
int port = 1883;

WiFiClient esp32Client;
PubSubClient client(esp32Client);

// ---- I2C helpers ----
void mpuWriteReg(uint8_t reg, uint8_t val){
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg); Wire.write(val);
  Wire.endTransmission();
}

void mpuReadBurst(uint8_t startReg, uint8_t *buf, size_t n){
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(startReg);
  Wire.endTransmission(false);
  Wire.requestFrom((int)MPU_ADDR, (int)n, true);
  for(size_t i=0; i<n && Wire.available(); ++i) buf[i] = Wire.read();
}

// median of 3 ints (branch-light)
int median3(int a, int b, int c){
  if (a > b) { int t=a; a=b; b=t; }
  if (b > c) { int t=b; b=c; c=t; }
  if (a > b) { int t=a; a=b; b=t; }
  return b;
}

// --- Comms functions ---
void connect_wifi() {
  WiFi.begin(ssid, password);
  Serial.print("\nConnecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(500);
  }
  Serial.println("\nConnected to the WiFi network");
  Serial.print("ESP32 IP: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  // Loop until connected
  while (!client.connected()) {
    if (client.connect("ESP32_Client")) {
      Serial.println("Connected to MQTT Broker");

    } else {
      Serial.println("Failed to connect");
      Serial.println(client.state());
      Serial.println("Trying again in 5s...");
      delay(5000);
    }
  }
}

void setup(){
  Serial.begin(115200);
  delay(100);

  // Connect to WiFi
  connect_wifi();

  // Connect to MQTT broker
  client.setServer(mqttServer, port);

  // I2C (FireBeetle ESP32 default pins 21=SDA, 22=SCL)
  Wire.begin(21,22);
  delay(50);

  // Wake MPU + set ranges
  mpuWriteReg(REG_PWR_MGMT_1, 0x00);
  delay(100);
  mpuWriteReg(REG_GYRO_CONFIG,  0x00); // ±250 dps
  mpuWriteReg(REG_ACCEL_CONFIG, 0x00); // ±2 g
  delay(50);

  // ADC config
  analogReadResolution(12); // 0..4095
  for (int i=0;i<5;i++) analogSetPinAttenuation(FLEX_PINS[i], ADC_11db); // up to ~3.6V

  // ---- Calibration (keep glove still & relaxed) ----
  Serial.println("# Calibrating... keep glove still & relaxed");
  uint32_t t0 = millis();
  uint32_t n = 0;
  double gxSum=0, gySum=0, gzSum=0;
  double axSum=0, aySum=0, azSum=0;
  uint32_t flexSum[5] = {0,0,0,0,0};

  while (millis()-t0 < CALIB_MS){
    uint8_t raw[14];
    mpuReadBurst(REG_ACCEL_XOUT_H, raw, 14);

    int16_t ax_raw = (int16_t)((raw[0]<<8)|raw[1]);
    int16_t ay_raw = (int16_t)((raw[2]<<8)|raw[3]);
    int16_t az_raw = (int16_t)((raw[4]<<8)|raw[5]);
    int16_t gx_raw = (int16_t)((raw[8]<<8)|raw[9]);
    int16_t gy_raw = (int16_t)((raw[10]<<8)|raw[11]);
    int16_t gz_raw = (int16_t)((raw[12]<<8)|raw[13]);

    gxSum += gx_raw; gySum += gy_raw; gzSum += gz_raw;
    axSum += (float)ax_raw/ACCEL_SENS;
    aySum += (float)ay_raw/ACCEL_SENS;
    azSum += (float)az_raw/ACCEL_SENS;

    for (int i=0;i<5;i++) flexSum[i] += analogRead(FLEX_PINS[i]);

    n++;
    delay(5);
  }
  if(n==0) n=1;
  gyroBias[0]=gxSum/n; gyroBias[1]=gySum/n; gyroBias[2]=gzSum/n;

  ax_f = axSum/n; ay_f = aySum/n; az_f = azSum/n;

  // init angles from accel (yaw=0 by definition, will drift over time)
  roll  = atan2f(ay_f, az_f) * 180.0f/PI;
  pitch = atan2f(-ax_f, sqrtf(ay_f*ay_f + az_f*az_f)) * 180.0f/PI;
  yaw   = 0.0f;

  // init flex baselines + EMA
  for (int i=0;i<5;i++){
    flexBase[i] = (int)(flexSum[i]/n);
    int centered = 0;
    flexEMA[i] = 0;            // start at zeroed baseline
    fHist[i][0] = fHist[i][1] = fHist[i][2] = centered;
  }
  fIdx = 0;

  lastMicros = micros();
  Serial.println("# Calibration done. Press ENTER to pause/resume.");
}

void loop(){
  // ---- Pause/resume with ENTER (LF/CR) ----
  if (Serial.available()){
    char c = Serial.read();
    if (c=='\n' || c=='\r'){
      paused = !paused;
      Serial.println(paused ? "\n# === PAUSED ===\n" : "\n# === RESUMED ===\n");
    }
  }
  if (paused){ delay(40); return; }

  // ---- Timing ----
  uint32_t now = micros();
  float dt = (now - lastMicros) * 1e-6f;
  if (dt <= 0) dt = 1e-3f;
  lastMicros = now;

  // ---- Read MPU ----
  uint8_t raw[14];
  mpuReadBurst(REG_ACCEL_XOUT_H, raw, 14);
  int16_t ax_raw = (int16_t)((raw[0]<<8)|raw[1]);
  int16_t ay_raw = (int16_t)((raw[2]<<8)|raw[3]);
  int16_t az_raw = (int16_t)((raw[4]<<8)|raw[5]);
  int16_t gx_raw = (int16_t)((raw[8]<<8)|raw[9]);
  int16_t gy_raw = (int16_t)((raw[10]<<8)|raw[11]);
  int16_t gz_raw = (int16_t)((raw[12]<<8)|raw[13]);

  float ax = (float)ax_raw / ACCEL_SENS;                    // g
  float ay = (float)ay_raw / ACCEL_SENS;
  float az = (float)az_raw / ACCEL_SENS;
  float gx = ((float)gx_raw - gyroBias[0]) / GYRO_SENS;     // deg/s
  float gy = ((float)gy_raw - gyroBias[1]) / GYRO_SENS;
  float gz = ((float)gz_raw - gyroBias[2]) / GYRO_SENS;

  // ---- Accel low-pass ----
  ax_f = ACC_LP*ax + (1.0f-ACC_LP)*ax_f;
  ay_f = ACC_LP*ay + (1.0f-ACC_LP)*ay_f;
  az_f = ACC_LP*az + (1.0f-ACC_LP)*az_f;

  // ---- Tilt from accel ----
  float roll_acc  = atan2f(ay_f, az_f) * 180.0f/PI;
  float pitch_acc = atan2f(-ax_f, sqrtf(ay_f*ay_f + az_f*az_f)) * 180.0f/PI;

  // ---- Gyro integrate ----
  float roll_gyro  = roll  + gx*dt;
  float pitch_gyro = pitch + gy*dt;
  float yaw_gyro   = yaw   + gz*dt; // (drifts over time)

  // ---- Complementary fuse ----
  roll  = CF_ALPHA*roll_gyro  + (1.0f-CF_ALPHA)*roll_acc;
  pitch = CF_ALPHA*pitch_gyro + (1.0f-CF_ALPHA)*pitch_acc;
  yaw   = yaw_gyro;

  if (yaw >= 180.0f) yaw -= 360.0f;
  else if (yaw < -180.0f) yaw += 360.0f;

  // ---- Flex read -> median-of-3 -> baseline -> EMA ----
  fIdx = (fIdx + 1) % 3;
  float fOut[5];
  for (int i=0;i<5;i++){
    int rawAdc = analogRead(FLEX_PINS[i]);
    fHist[i][fIdx] = rawAdc;
    int med = median3(fHist[i][0], fHist[i][1], fHist[i][2]); // spike reject
    int centered = med - flexBase[i];                         // zero at relaxed
    // optional clamp to reduce wild swings (comment out if not needed)
    if (centered > 3000) centered = 3000;
    if (centered < -3000) centered = -3000;
    flexEMA[i] = FLEX_ALPHA*centered + (1.0f - FLEX_ALPHA)*flexEMA[i];
    fOut[i] = flexEMA[i]; // smoothed, baseline-removed counts
  }

  // MQTT message payload
  String payload = "{";
  payload += "\"roll\":"; payload += String(roll, 2); payload += ",";
  payload += "\"pitch\":"; payload += String(pitch, 2); payload += ",";
  payload += "\"yaw\":"; payload += String(yaw, 2); payload += ",";
  payload += "\"ax_f\":"; payload += String(ax_f, 3); payload += ",";
  payload += "\"ay_f\":"; payload += String(ay_f, 3); payload += ",";
  payload += "\"az_f\":"; payload += String(az_f, 3); payload += ",";
  payload += "\"flex1\":"; payload += String(fOut[0], 1); payload += ",";
  payload += "\"flex2\":"; payload += String(fOut[1], 1); payload += ",";
  payload += "\"flex3\":"; payload += String(fOut[2], 1); payload += ",";
  payload += "\"flex4\":"; payload += String(fOut[3], 1); payload += ",";
  payload += "\"flex5\":"; payload += String(fOut[4], 1);
  payload += "}";

  client.publish("glove/data", payload.c_str());

  // ---- Print one CSV line ----
  // Serial.print(roll, 2);  Serial.print(',');
  // Serial.print(pitch, 2); Serial.print(',');
  // Serial.print(yaw, 2);   Serial.print(',');
  // Serial.print(ax_f, 3);  Serial.print(',');
  // Serial.print(ay_f, 3);  Serial.print(',');
  // Serial.print(az_f, 3);  Serial.print(',');
  // for (int i=0;i<5;i++){
  //   Serial.print(fOut[i], 1);
  //   if (i<4) Serial.print(',');
  // }
  // Serial.println();

  // ~100 Hz
  delay(10);
}
