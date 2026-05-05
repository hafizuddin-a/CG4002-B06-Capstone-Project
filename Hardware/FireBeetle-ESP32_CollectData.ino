// FireBeetle ESP32 V3 + MPU6050 (I2C) + 5 Flex Sensors (A0..A4)
// lexFilteredput one CSV line: roll,pitch,yaw,ax,ay,az,f0,f1,f2,f3,f4
// Press ENTER in Serial Monitor to pause/resume streaming.

#include <Wire.h>
#include <math.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include "time.h"

// ---- MPU6050 ----
static const uint8_t MPU_ADDR = 0x68;
static const uint8_t REG_PWR_MGMT_1   = 0x6B;
static const uint8_t REG_ACCEL_XlexFiltered_H = 0x3B;
static const uint8_t REG_GYRO_CONFIG  = 0x1B;
static const uint8_t REG_ACCEL_CONFIG = 0x1C;

static const float ACCEL_SENS = 16384.0f;  // LSB/g at ±2g
static const float GYRO_SENS  = 131.0f;    // LSB/(deg/s) at ±250 dps

// ---- Filters / tuning ----
static const float CF_ALPHA_ROLL = 0.96f;   // Roll trusts gyro more
static const float CF_ALPHA_PITCH = 0.94f;  // Pitch trusts accel more
static const float ACC_LP   = 0.15f;   // accel EMA low-pass (0..1)
static const uint32_t CALIB_MS = 1000; // keep still during this

// ---- Pins ----
const int FLEX_PINS[5] = {A0, A1, A2, A3, A4};

#define TOUCH_PIN_MID 25 
#define TOUCH_PIN_TIP 26

// ---- State ----
float gyroBias[3] = {0,0,0};
float ax_f=0, ay_f=0, az_f=0;          // accel LP
float roll=0, pitch=0, yaw=0;          // deg

static int data_collection_counter = 0;
static const int data_collection_timesteps = 100;

int   flexBase[5] = {0,0,0,0,0};       // baseline (relaxed hand)
float flexFiltered[5]  = {0,0,0,0,0};       // smoothed centered values

float filteredLPF[5] = {0, 0, 0, 0, 0};
float filteredHPF[5] = {0, 0, 0, 0, 0};

// median-of-3 buffers for flex (spike suppression)
int   fHist[5][3] = {0};               // circular history
uint8_t fIdx = 0;

uint32_t lastMicros = 0;

// Add after the existing state variables (around line 30)
#define MAX_CALIB_SAMPLES 1000
int flexStraightSamples[5][MAX_CALIB_SAMPLES];
int flexBentSamples[5][MAX_CALIB_SAMPLES];

int flexStraight[5] = {0,0,0,0,0};  // Median values when straight
int flexBent[5] = {0,0,0,0,0};      // Median values when bent

// Wi-Fi and MQTT credentials
const char* ssid     = "Galaxy S21";
const char* password = "thjy0597";
const char* mqttServer = "10.224.94.195";  // Replace with your server's IP
const int port = 8883;

#define LED_PIN 2

bool isReady = false;
int latencyCounter = 0;

// CA certificate
const char* ca_cert = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIDzTCCArWgAwIBAgIUKjBlHukWj01EZCC07IMqxwgzxhkwDQYJKoZIhvcNAQEL\n" \
"BQAwdjELMAkGA1UEBhMCU0cxEjAQBgNVBAgMCVNpbmdhcG9yZTESMBAGA1UEBwwJ\n" \
"U2luZ2Fwb3JlMQwwCgYDVQQKDANOVVMxGTAXBgNVBAsMEENHNDAwMiBHcm91cCBC\n" \
"MDYxFjAUBgNVBAMMDTEwLjIyNC45NC4xOTUwHhcNMjUxMTAzMDYxMjAzWhcNMzUx\n" \
"MTAxMDYxMjAzWjB2MQswCQYDVQQGEwJTRzESMBAGA1UECAwJU2luZ2Fwb3JlMRIw\n" \
"EAYDVQQHDAlTaW5nYXBvcmUxDDAKBgNVBAoMA05VUzEZMBcGA1UECwwQQ0c0MDAy\n" \
"IEdyb3VwIEIwNjEWMBQGA1UEAwwNMTAuMjI0Ljk0LjE5NTCCASIwDQYJKoZIhvcN\n" \
"AQEBBQADggEPADCCAQoCggEBAKHu3J8UshVLGWGVYKgZxXHRp8H8WIIsSJXK35hd\n" \
"nH0RNavmLc30qEbtobN7OTYkfMQG5YiVR2I+xPDhtasN+alHtv7zTVUPg01Rhulp\n" \
"DYqqdi032H+q9z7B/3BWexsxnG/vDlX1X/BU4Yfb8diF7oRMyqkcO/2yjhlcOCX6\n" \
"uHAqpV9uMe5GzD81cxlOM5o99NELnMFm5j3Bu831J9FanAUlS2QlLX0hjaLxcFbA\n" \
"0tNZulWMfMukKqMQr2IJ4u5TFU2blgEU7VUhF5BkyO/Atjx0JCx+JchsP6cXiIG+\n" \
"ABT1MWsilEuNJLIBLEB0h6DC3p5fMeQgLHpU3Pblj0W8suMCAwEAAaNTMFEwHQYD\n" \
"VR0OBBYEFGn8h14qNKhIjN/JyL/nhvKcZk0wMB8GA1UdIwQYMBaAFGn8h14qNKhI\n" \
"jN/JyL/nhvKcZk0wMA8GA1UdEwEB/wQFMAMBAf8wDQYJKoZIhvcNAQELBQADggEB\n" \
"AB4+v1VKpvokelVJyoQSTw4UxrOGAHRfqg6Tz6lRGHOCmX7NUEwJ3ZFutwuWp/i2\n" \
"/zUFu7hbF5kvYQYlO66lZpzul7RuehO+m/PV2A6hue6blw4cLLfzZktgClHhpV+U\n" \
"I/jVBMyYWQ83qr9OOPj7psZJCGvMXOhlNgIVfdy+gztLYpVa8mSmbVAYC4TzHFmY\n" \
"QihIqGyvJjEndXLlYzws21/TTjSYMS/12QRqGVbXEAhImLxW8VYrOFGkrq4494PY\n" \
"+PDCjlvaeiXtPoqAdYS2A6CSWlKRR1QxT9crhF/gmmayv0ITSpCuebsn8wfBq+7U\n" \
"gtZ8c5QgGmdeTR0ToNJuOlA=\n" \
"-----END CERTIFICATE-----";

// ESP32 client certificate and private key
const char* client_cert = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIDuzCCAqOgAwIBAgIUL7UBggEJ67BPxWLzbusPQMjUzKcwDQYJKoZIhvcNAQEL\n" \
"BQAwdjELMAkGA1UEBhMCU0cxEjAQBgNVBAgMCVNpbmdhcG9yZTESMBAGA1UEBwwJ\n" \
"U2luZ2Fwb3JlMQwwCgYDVQQKDANOVVMxGTAXBgNVBAsMEENHNDAwMiBHcm91cCBC\n" \
"MDYxFjAUBgNVBAMMDTEwLjIyNC45NC4xOTUwHhcNMjUxMTA1MTUzNDQzWhcNMjYx\n" \
"MTA1MTUzNDQzWjB1MQswCQYDVQQGEwJTRzESMBAGA1UECAwJU2luZ2Fwb3JlMRIw\n" \
"EAYDVQQHDAlTaW5nYXBvcmUxDDAKBgNVBAoMA05VUzEZMBcGA1UECwwQQ0c0MDAy\n" \
"IEdyb3VwIEIwNjEVMBMGA1UEAwwMRVNQMzIgY2xpZW50MIIBIjANBgkqhkiG9w0B\n" \
"AQEFAAOCAQ8AMIIBCgKCAQEAngPX4P+ueokUBbhVIy/TSAl1yYjw3jaITl7V9v+r\n" \
"umGNKNXCKE8P4LZWcYlsITS0cKrIuxSq1oEmXvr/ewZXrJ6K+PJSfbBJcwjWVR48\n" \
"AmZBFdKcSzXEY0dYeeAEW3z6VUNAjmlqnadiuE3VdK6AjrsFWiR0j5M5N37uZi5N\n" \
"CLk6IivxTM7hV2xVrjKaEbu9h9w1mVzEUDA41lz4Ctny1MMM+H6329ho/DG+s4M9\n" \
"AQSxCqGyFhA/LvUPXOL1hUJCg3qBZywbJhnoBxx6KSngd7/5PO+LaIImyKuyT0c+\n" \
"ymMX1WlKXZeYNfYxiicGzSVyjebnMavPaydhuVEIPp+JZwIDAQABo0IwQDAdBgNV\n" \
"HQ4EFgQUTzIlPQsu2cn6BkwNC1UINvlnj3YwHwYDVR0jBBgwFoAUafyHXio0qEiM\n" \
"38nIv+eG8pxmTTAwDQYJKoZIhvcNAQELBQADggEBAEKXcWl/HJyve29U3q3JeOgR\n" \
"G/Juh74UKKpGmts/VHYgxEMwyyMchOjZqvZbAS+Gpcfp3LACMofNbXnlWtQa51f7\n" \
"HS6hBeCC91Pp4FW2K//swn89jKLkpXZDySZ2Zoc97n6gEihEiVMTye6fYjrSJpzF\n" \
"dUUDb2irQcZRm3CMB8nmMzPRtEl1p17/sas8BMkd6WB0lLObgyCYkMiWVn2jE7YO\n" \
"VqccSNI/4IkYckcJXDMDBGDwo/1AC+eVpoAWrzrmNOVleI9ziM6wpYX4GT8z/Exd\n" \
"k138XXexEIU/gSjHUwrgRd4BfsWdyQXc7CH2IAuEFm0eXIM6j53AXywE05SSSCE=\n" \
"-----END CERTIFICATE-----";

const char* client_key = \
"-----BEGIN PRIVATE KEY-----\n" \
"MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQCeA9fg/656iRQF\n" \
"uFUjL9NICXXJiPDeNohOXtX2/6u6YY0o1cIoTw/gtlZxiWwhNLRwqsi7FKrWgSZe\n" \
"+v97Blesnor48lJ9sElzCNZVHjwCZkEV0pxLNcRjR1h54ARbfPpVQ0COaWqdp2K4\n" \
"TdV0roCOuwVaJHSPkzk3fu5mLk0IuToiK/FMzuFXbFWuMpoRu72H3DWZXMRQMDjW\n" \
"XPgK2fLUwwz4frfb2Gj8Mb6zgz0BBLEKobIWED8u9Q9c4vWFQkKDeoFnLBsmGegH\n" \
"HHopKeB3v/k874togibIq7JPRz7KYxfVaUpdl5g19jGKJwbNJXKN5ucxq89rJ2G5\n" \
"UQg+n4lnAgMBAAECggEAOeB1f3uXKFmyJEA2qus3C6Eva/CFLRczSOVSoKRX/a4C\n" \
"eq0E7ye6TJfsxKxNl0ILP2NGn8N7qZLnf42W6zRRA7CUfYegcFoUJRbdDpNC7qwO\n" \
"ddRNG/0nICf/P7CuV+ZIeNdnu2HgQ7uOHKyhnRnXi6/zz2cf7IDDydBruLCH1cQT\n" \
"LhmnFG4NcF8ER2FZZTnCcmL/krbPQy8BZhl2XBVtyrRicnkkntH6ZMai8fyjyJmA\n" \
"VIh/6yoL3x6ETx/4z9gmIivKCRpfYM3sdYegAZKmQBMoj8rBpv8Xjk6yjI5vQdQn\n" \
"pEzzD3SaMuMbVEhtYDPGBNr6TgRLjmdf8ZQbDVVDRQKBgQDLtuzPAEY30ypGDS7y\n" \
"KtjwlampnqdMBZgizP+EmkuLSvm288FqwaFUpGttFybFIvjcALlcR6tkBr6hbzwm\n" \
"RR1dE55VixT5VGXSn1QXTHoE4MVVADRsRX9Pzm/GuaxCuCtQPEhKUX3gM/zq1APT\n" \
"RXT4nZ4LmOCveYAr+jzU1105VQKBgQDGkjpOaZf5AwpDDLyZfj4ZVIgWqQ+5p7sp\n" \
"Sl99Ar6T/LUAkGXL7VDb6HecdvsLKkpLvyqCjE+MNco4m9se1Q+E5v2TgWTIIJ5n\n" \
"pX+P5vSSdbIT5Diy3GykszGq7rbWYSHM1GQ4l6P8CwZX7OiTb54cAhg5p2iHdrjn\n" \
"fVSpd5rHywKBgQCG6QwHQr0990DFx7FRe4LUQalsxb8xn3rTgUOvA6gIBY0+1ks/\n" \
"ciBvt7vVMUHhyHla6bfYHzaoNbdFni8NgTQjEf0H1eX8ASK1zAKas3ETje/hjRMq\n" \
"qRPZPHPV/dzRCrrUljeh0Do2ovdaZTJrVlJS279xGruiOirh9QUYR9BbSQKBgBMf\n" \
"8k2rDeAF0u8yQtfluMVnxtOn5MPUy8nmR9wab5CBPk2XmrZRXQBRG3QOYY9pu04g\n" \
"U0/Pg7nVVGyvViNaEeyS8slKJRLBUYBaCDcr31Jb05Lm4C8Z27zhJV95LlLneAHq\n" \
"UtzAfiLATRQa1SueDHRWH08uOHsTjCt/fq/zvQyRAoGBAK+MuRb+L4lrULfVvDiq\n" \
"I5QIsxBoDDUPEu0rFiQx+P6FUTH3CRF8qA0JlVDhHL9kwwP+NydslAPCwjhtJNtR\n" \
"qmfh1Sq5cuqjYigIVQO8wqnzUCgOFNM37NrsekH6M3ax8ZPX6TUvA61EY/XENWfb\n" \
"u7SDFTQSnqK++pVdSgcaLfXK\n" \
"-----END PRIVATE KEY-----";

WiFiClientSecure esp32Client;
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

  // Add after the median3() function
  int calculateMedian(int* arr, int count) {
    // Simple selection sort (good enough for ~1000 samples)
    int* temp = (int*)malloc(count * sizeof(int));
    if (temp == NULL) return 0; // Memory allocation failed
    
    memcpy(temp, arr, count * sizeof(int));
    
    // Sort
    for (int i = 0; i < count - 1; i++) {
      for (int j = i + 1; j < count; j++) {
        if (temp[i] > temp[j]) {
          int swap = temp[i];
          temp[i] = temp[j];
          temp[j] = swap;
        }
      }
    }
    
    int median = temp[count / 2];
    free(temp);
    return median;
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

void sendTime() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  unsigned long long t1 = (unsigned long long)tv.tv_sec * 1000ULL + tv.tv_usec / 1000ULL;
  if (t1 > 17000000) {
    client.publish("latency/time/esp32", String(t1).c_str());
  }

}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
  Serial.begin(115200);
  delay(100);

  // Connect to WiFi
  connect_wifi();

  // Set certificates
  esp32Client.setCACert(ca_cert);
  esp32Client.setCertificate(client_cert);
  esp32Client.setPrivateKey(client_key);

  // Connect to MQTT broker
  client.setServer(mqttServer, port);

  reconnect();
  client.loop();  // process connection packets
  uint32_t lastMqttLoop = millis();

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

  // ---- PHASE 1: Calibrate with STRAIGHT fingers ----
  client.publish("glove/status/unity", "Calibration phase 1 start:\nKeep glove still with fingers STRAIGHT for 5 seconds...");
  // Print messages
  Serial.println("# CALIBRATION PHASE 1:");
  Serial.println("# Keep glove still with fingers STRAIGHT for 5 seconds...");
  delay(2000); // Give user time to read
  
  uint32_t t0 = millis();
  uint32_t n = 0;
  double gxSum=0, gySum=0, gzSum=0;
  double axSum=0, aySum=0, azSum=0;

  while (millis()-t0 < 5000 && n < MAX_CALIB_SAMPLES){ // 5 seconds
    uint8_t raw[14];
    mpuReadBurst(REG_ACCEL_XlexFiltered_H, raw, 14);

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

    // Store all flex samples for median calculation
    for (int i=0;i<5;i++) {
      flexStraightSamples[i][n] = analogRead(FLEX_PINS[i]);
    }

    n++;

    // Maintain MQTT connection
    if (millis() - lastMqttLoop > 1000) {
      client.loop();  // keep MQTT alive
      lastMqttLoop = millis();
    }
    delay(5);
  }
  
  if(n==0) n=1;
  gyroBias[0]=gxSum/n; gyroBias[1]=gySum/n; gyroBias[2]=gzSum/n;
  ax_f = axSum/n; ay_f = aySum/n; az_f = azSum/n;

  // Calculate MEDIAN for straight fingers
  Serial.println("# Phase 1 complete!");
  Serial.print("# Straight finger medians (from "); 
  Serial.print(n); 
  Serial.println(" samples):");
  String message = "# Phase 1 complete!\n# Straight finger medians (from ";
  message += String(n); message += " samples):\n";
  
  for (int i=0;i<5;i++){
    flexStraight[i] = calculateMedian(flexStraightSamples[i], n);
    flexBase[i] = flexStraight[i]; // Set baseline
    
    Serial.print("#   Flex "); Serial.print(i); 
    Serial.print(": "); Serial.println(flexStraight[i]);
    message = "";
    message += "#   Flex "; message += String(i);
    message += ": "; message += String(flexStraight[i]); message += "\n";
    client.publish("glove/status", message.c_str());
  }


  // init angles from accel
  roll  = atan2f(ay_f, az_f) * 180.0f/PI;
  pitch = atan2f(-ax_f, sqrtf(ay_f*ay_f + az_f*az_f)) * 180.0f/PI;
  yaw   = 0.0f;

  // ---- PHASE 2: Calibrate with BENT fingers ----
  client.publish("glove/status/unity", "Calibration phase 2 start:\nNow BEND all fingers fully for 5 seconds...");
  // Print messages
  Serial.println("# CALIBRATION PHASE 2:");
  Serial.println("# Now BEND all fingers fully for 5 seconds...");
  delay(3000); // Give user time to bend fingers
  
  t0 = millis();
  n = 0;

  while (millis()-t0 < 5000 && n < MAX_CALIB_SAMPLES){ // 5 seconds
    for (int i=0;i<5;i++) {
      flexBentSamples[i][n] = analogRead(FLEX_PINS[i]);
    }
    n++;
    // Maintain MQTT connection
    if (millis() - lastMqttLoop > 1000) {
      client.loop();  // keep MQTT alive
      lastMqttLoop = millis();
    }
    delay(5);
  }

  // Calculate MEDIAN for bent fingers
  Serial.println("# Phase 2 complete!");
  Serial.print("# Bent finger medians (from "); 
  Serial.print(n); 
  Serial.println(" samples):");
  message = "";
  message += "# Phase 2 complete!\n# Bent finger medians (from ";
  message += String(n); message += " samples):\n";
  client.publish("glove/status", message.c_str());
  
  for (int i=0;i<5;i++){
    flexBent[i] = calculateMedian(flexBentSamples[i], n);
    
    Serial.print("#   Flex "); Serial.print(i);
    Serial.print(": "); Serial.println(flexBent[i]);
    message = "";
    message += "#   Flex "; message += String(i);
    message += ": "; message += flexBent[i];
    client.publish("glove/status", message.c_str());
  }

  // Print calibration range
  Serial.println("# Calibration ranges:");
  client.publish("glove/status", "# Calibration ranges:");
  for (int i=0;i<5;i++){
    Serial.print("#   Flex "); Serial.print(i);
    Serial.print(": "); Serial.print(flexStraight[i]);
    Serial.print(" -> "); Serial.print(flexBent[i]);
    Serial.print(" (range: "); Serial.print(flexBent[i] - flexStraight[i]);
    Serial.println(")");
    message = "";
    message += "#   Flex ";
    message += String(i);
    message += ": ";
    message += String(flexStraight[i]);
    message += " -> ";
    message += String(flexBent[i]);
    message += " (range: ";
    message += String(flexBent[i] - flexStraight[i]);
    message += ")\n";   // add newline for readability
    client.publish("glove/status", message.c_str());
  }

  // Initialize history buffers
  for (int i=0;i<5;i++){
    fHist[i][0] = fHist[i][1] = fHist[i][2] = 0;
  }
  fIdx = 0;

  lastMicros = micros();
  Serial.println("# Calibration complete! Starting data stream...");
  Serial.println("# Press ENTER to pause/resume.");
  client.publish("glove/status/unity", "Calibration complete! Starting game in 5 seconds...\nLOOK STRAIGHT AT THE IMAGE");

  pinMode(TOUCH_PIN_MID, INPUT_PULLUP);
  pinMode(TOUCH_PIN_TIP, INPUT_PULLUP);

  configTime(8 * 3600, 0, "pool.ntp.org"); // Singapore time offset = +8h
  delay(2000);
}

void loop(){
  // ---- Connect to MQTT broker ----
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // ---- Pause/resume with ENTER (LF/CR) ----
  if (Serial.available()){
    char c = Serial.read();
    if (c=='\n' || c=='\r'){
      data_collection_counter = 0;
    }
  }
 
  // ---- Timing ----
  uint32_t now = micros();
  float dt = (now - lastMicros) * 1e-6f;
  if (dt <= 0) dt = 1e-3f;
  lastMicros = now;

  // ---- Read MPU ----
  uint8_t raw[14];
  mpuReadBurst(REG_ACCEL_XlexFiltered_H, raw, 14);
  int16_t ax_raw = (int16_t)((raw[0]<<8)|raw[1]);
  int16_t ay_raw = (int16_t)((raw[2]<<8)|raw[3]);
  int16_t az_raw = (int16_t)((raw[4]<<8)|raw[5]);
  int16_t gx_raw = (int16_t)((raw[8]<<8)|raw[9]);
  int16_t gy_raw = (int16_t)((raw[10]<<8)|raw[11]);
  int16_t gz_raw = (int16_t)((raw[12]<<8)|raw[13]);

  float ax = (float)ax_raw / ACCEL_SENS;                    // g
  float ay = -(float)ay_raw / ACCEL_SENS;
  float az = -(float)az_raw / ACCEL_SENS;
  float gx = ((float)gx_raw - gyroBias[0]) / GYRO_SENS;     // deg/s
  float gy = -((float)gy_raw - gyroBias[1]) / GYRO_SENS;
  float gz = -((float)gz_raw - gyroBias[2]) / GYRO_SENS;

  // ---- Accel low-pass ----
  ax_f = ACC_LP*ax + (1.0f-ACC_LP)*ax_f;
  ay_f = ACC_LP*ay + (1.0f-ACC_LP)*ay_f;
  az_f = ACC_LP*az + (1.0f-ACC_LP)*az_f;

  // ---- Tilt from accel ----
  float roll_acc = atan2f(ay_f, az_f) * 180.0f/PI;
  float pitch_acc = atan2f(-ax_f, sqrtf(ay_f*ay_f + az_f*az_f)) * 180.0f/PI;

  // ---- Gyro integrate ----
  float roll_gyro  = roll  + gx*dt;
  float pitch_gyro = pitch + gy*dt;
  float yaw_gyro   = yaw   + gz*dt; // (drifts over time)

  // ---- Complementary fuse ----
  roll = CF_ALPHA_ROLL * roll_gyro + (1.0f - CF_ALPHA_ROLL) * roll_acc;
  pitch = CF_ALPHA_PITCH * pitch_gyro + (1.0f - CF_ALPHA_PITCH) * pitch_acc;
  yaw   = yaw_gyro;

  if (yaw >= 180.0f) yaw -= 360.0f;
  else if (yaw < -180.0f) yaw += 360.0f;

  // Flex read with normalization
  static float const FLEX_SMOOTH = 0.5f; // Smoothing factor (0-1, lower = smoother)

  fIdx = (fIdx + 1) % 3;
  for (int i=0;i<5;i++){
    int rawAdc = analogRead(FLEX_PINS[i]);
    
    // Normalize to 0-100 range using calibrated medians
    // 0 = straight, 100 = fully bent
    float normalized = 0.0f;
    
    if (flexBent[i] != flexStraight[i]) { // Avoid division by zero
      normalized = ((float)(rawAdc - flexStraight[i]) / (float)(flexBent[i] - flexStraight[i])) * 100.0f;
      
      // Clamp to 0-100 range
      if (normalized < 0.0f) normalized = 0.0f;
      if (normalized > 100.0f) normalized = 100.0f;
    }
    
    flexFiltered[i] = FLEX_SMOOTH * normalized + (1.0f - FLEX_SMOOTH) * flexFiltered[i];
  }

  bool touchingMid = digitalRead(TOUCH_PIN_MID);
  bool touchingTip = digitalRead(TOUCH_PIN_TIP);

  // ---- Print {data_collection_timesteps} CSV lines, 11 values per line ----
  // if (data_collection_counter >= 0 && data_collection_counter < data_collection_timesteps) {
  //   String(roll, 2);  String(',');
  //   String(pitch, 2); String(',');
  //   String(yaw, 2);   String(',');
  //   String(ax_f, 3);  String(',');
  //   String(ay_f, 3);  String(',');
  //   String(az_f, 3);  String(',');
    
  //   if (touchingMid == LOW) {
  //     String("1");
  //   } else {
  //     String("0");
  //   }
  //   String(',');
    
  //   if (touchingTip == LOW) {
  //     String("1");
  //   } else {
  //     String("0");
  //   }
  //   String(',');

  //   for (int i = 0; i < 5; i++) {
  //     String(flexFiltered[i], 1);
  //     if (i < 4) String(',');
  //   }
  //   //Serial.println();

  //   // After 20th timestep, end the line
  //   // Else, print ',' after every timestep
  //   if (data_collection_counter == data_collection_timesteps - 1) {
  //     Serial.println(); // Send the full line to serial
  //     data_collection_counter = -1;
  //   } else {
  //     String(',');
  //     data_collection_counter++;
  //   }
  // }

  // MQTT message payload
  String payload = "[";
  payload += String(roll, 2); payload += ", ";
  payload += String(pitch, 2); payload += ", ";
  payload += String(yaw, 2); payload += ", ";
  payload += String(ax_f, 3); payload += ", ";
  payload += String(ay_f, 3); payload += ", ";
  payload += String(az_f, 3); payload += ", ";
  if (touchingMid == LOW) {
      payload += String("1"); payload += ", ";
    } else {
      payload += String("0"); payload += ", ";
    }
  if (touchingTip == LOW) {
      payload += String("1"); payload += ", ";
    } else {
      payload += String("0"); payload += ", ";
    }
  payload += String(flexFiltered[0], 1); payload += ", ";
  payload += String(flexFiltered[1], 1); payload += ", ";
  payload += String(flexFiltered[2], 1); payload += ", ";
  payload += String(flexFiltered[3], 1); payload += ", ";
  payload += "]";

  client.publish("glove/data", payload.c_str());
  if (latencyCounter % 99 == 0) {
    sendTime();
    latencyCounter = 0;
  }
  latencyCounter += 1;
  
  
  // ~100 Hz
  delay(10);
}
