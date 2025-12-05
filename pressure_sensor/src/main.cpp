#include <Arduino.h>

// HX710B Air Pressure Sensor with Arduino 
// Pin connections:
// HX710B DT  -> Arduino pin 2
// HX710B SCK -> Arduino pin 3
// VCC -> 3.3V or 5V (check your module specs)
// GND -> GND

#define DT  2
#define SCK 3

// ---- Physical constants ----
const float GRAVITY = 9.80665;   // m/s^2

// EFFECTIVE BELLOWS AREA IN m^2
// Example: 1 cm^2  =>  1e-4 m^2
//          d = 10 mm => A ≈ 7.85e-5 m^2
// >>> CHANGE THIS TO YOUR REAL BELLOWS AREA <<<
const float BELLOWS_AREA_M2 = 1.44e-4;

// ---- Calibration variables ----
float rawZero = 0.0;                    // raw counts at zero weight
float rawCal  = 0.0;                    // raw counts at known weight
float rawToPressure_PaPerCount = 0.0;   // slope: Pa per ADC count
float calibrationWeight_g = 0.0;        // known mass in grams

// ----------- HX710B read function -----------
long readHX710B() {
  long count = 0;
  unsigned long start = micros();

  // Wait for data ready (DT goes LOW)
  while (digitalRead(DT)) {
    if (micros() - start > 1000000) return -1; // timeout after ~1s
  }

  // Read 24 bits of data
  for (int i = 0; i < 24; i++) {
    digitalWrite(SCK, HIGH);
    delayMicroseconds(1);
    count = count << 1;
    digitalWrite(SCK, LOW);
    delayMicroseconds(1);
    if (digitalRead(DT)) count++;
  }

  // Extra clock pulse to set gain/channel
  digitalWrite(SCK, HIGH);
  delayMicroseconds(1);
  digitalWrite(SCK, LOW);

  // Convert to signed 24-bit
  if (count & 0x800000) count |= ~0xFFFFFF;

  return count;
}

// Average N valid readings from HX710B
long readAverageHX710B(int samples) {
  long sum = 0;
  int valid = 0;

  while (valid < samples) {
    long val = readHX710B();
    if (val != -1) {
      sum += val;
      valid++;
    }
    // if val == -1, try again (timeout)
  }

  return sum / samples;
}

// Wait for user to hit Enter in Serial Monitor
void waitForEnter() {
  while (!Serial.available()) { }
  while (Serial.available()) { Serial.read(); } // clear buffer
}

void setup() {
  pinMode(SCK, OUTPUT);
  pinMode(DT, INPUT);

  Serial.begin(9600);
  // Optional: wait for Serial on boards like Leonardo
  while (!Serial) { }

  Serial.println(F("HX710B Bellow Calibration Demo"));
  Serial.println(F("--------------------------------"));

  // ---- Step 1: zero-load baseline ----
  Serial.println(F("\nStep 1: ZERO LOAD"));
  Serial.println(F("Make sure there is NO weight on the bellow."));
  Serial.println(F("Press Enter in the Serial Monitor when ready."));
  waitForEnter();

  rawZero = (float)readAverageHX710B(50);  // average of 50 samples
  Serial.print(F("rawZero = "));
  Serial.println(rawZero, 1);

  // ---- Step 2: known weight calibration ----
  Serial.println(F("\nStep 2: KNOWN WEIGHT"));
  Serial.println(F("Place a known weight on the bellow and leave it there."));
  Serial.println(F("Now type that weight in GRAMS (e.g. 100) and press Enter:"));

  while (!Serial.available()) { }
  calibrationWeight_g = Serial.parseFloat();
  while (Serial.available()) { Serial.read(); }  // clear rest of line

  Serial.print(F("Calibration weight = "));
  Serial.print(calibrationWeight_g, 2);
  Serial.println(F(" g"));

  delay(1000); // let the system settle with the weight applied

  rawCal = (float)readAverageHX710B(50);  // average of 50 samples
  Serial.print(F("rawCal = "));
  Serial.println(rawCal, 1);

  float deltaRaw = rawCal - rawZero;
  if (deltaRaw == 0.0f) {
    Serial.println(F("ERROR: deltaRaw is zero. Check sensor / wiring / weight."));
    rawToPressure_PaPerCount = 0.0f;
  } else {
    // F = m * g  (N)
    float weight_kg = calibrationWeight_g / 1000.0f;
    float F_N = weight_kg * GRAVITY;

    // F = ΔP * A  =>  ΔP = F / A
    float deltaP_Pa = F_N / BELLOWS_AREA_M2;  // Pa (gauge, relative to zero-load)

    // Linear map: ΔP = (PaPerCount) * Δraw
    rawToPressure_PaPerCount = deltaP_Pa / deltaRaw;

    Serial.println(F("\nCalibration complete."));
    Serial.print(F("rawToPressure_PaPerCount = "));
    Serial.print(rawToPressure_PaPerCount, 6);
    Serial.println(F(" Pa/count"));
  }

  Serial.println(F("\nStarting live readings...\n"));
}

void loop() {
  // Read current raw value
  long rawValue = readAverageHX710B(10);   // average of 10 samples
  float raw = (float)rawValue;

  // Gauge pressure relative to zero-load
  float deltaRaw = raw - rawZero;
  float deltaP_Pa = deltaRaw * rawToPressure_PaPerCount; // Pa
  float pressure_kPa = deltaP_Pa / 1000.0f;              // kPa

  // Force and detected weight
  float F_N = deltaP_Pa * BELLOWS_AREA_M2;  // F = ΔP * A
  float weight_kg = F_N / GRAVITY;
  float weight_g = weight_kg * 1000.0f;

  // Serial output: raw, pressure, detected weight
  Serial.print("Raw: ");
  Serial.print(rawValue);
  Serial.print("  Pressure: ");
  Serial.print(pressure_kPa, 3);
  Serial.print(" kPa  Detected weight: ");
  Serial.print(weight_g, 1);
  Serial.println(" g");

  delay(500);
}
