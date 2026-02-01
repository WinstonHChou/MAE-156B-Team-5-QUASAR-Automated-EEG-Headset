#include <Arduino.h>
#include <Wire.h>
#include "Adafruit_MPRLS.h"


// ---- Physical constants ----
#define GRAVITY 9.80665f                              // m/s^2
#define KPA_TO_PSI(x) ((x) / 6.8947572932f)           // 1 PSI = 6.8947572932 kPa
#define HPA_TO_KPA(x) ((x) / 10.0f)                   // 1 hPa = 0.1 kPa
#define GRAM_TO_NEWTON(x) ((x) / 1000.0f * GRAVITY)   // convert grams to Newtons
#define NEWTON_TO_GRAM(x) ((x) * 1000.0f / GRAVITY) // convert Newtons to grams
#define PA_TO_KPA(x) ((x) / 1000.0f)                  // 1 Pa = 0.001 kPa
// -----------------------------


// EFFECTIVE BELLOWS AREA IN m^2
// Example: 1 cm^2  =>  1e-4 m^2
//          d = 10 mm => A ≈ 7.85e-5 m^2
// >>> CHANGE THIS TO YOUR REAL BELLOWS AREA <<<
// #define BELLOWS_AREA_M2 1.44e-4f              // e.g., 1.44 cm^2 = 1.44e-4 m^2
// ------------------------------

// You dont *need* a reset and EOC pin for most uses, so we set to -1 and don't connect
#define RESET_PIN  -1  // set to any GPIO pin # to hard-reset on begin()
#define EOC_PIN    -1  // set to any GPIO pin to read end-of-conversion by pin
Adafruit_MPRLS mpr = Adafruit_MPRLS(RESET_PIN, EOC_PIN);


// ---- Calibration variables ----
float pressureZero_kPa        = 0.0f;             // raw counts at zero weight
float pressureCali_kPa        = 0.0f;             // raw counts at known weight
float forceToSensorRatio      = 0.0f;             // slope: Pa per ADC count
float calibrationWeight_g     = 0.0f;             // known mass in grams
// -------------------------------

// Wait for user to hit Enter in Serial Monitor
void waitForEnter() {
  while (!Serial.available()) { }
  while (Serial.available()) { Serial.read(); } // clear buffer
}

void setup() {
  Serial.begin(115200);
  Serial.println("MPRLS Load Cell Test");
  Serial.println("--------------------------------");

  if (! mpr.begin()) {
    Serial.println("Failed to communicate with MPRLS sensor, check wiring?");
    while (1) {
      delay(10);
    }
  }
  Serial.println("Found MPRLS sensor");

  // ---- Step 1: zero-load baseline ----
  Serial.println("Step 1: ZERO LOAD");
  Serial.println("Make sure there is NO weight on the bellow.");
  Serial.println("Press Enter in the Serial Monitor when ready.");
  // waitForEnter();

  pressureZero_kPa = HPA_TO_KPA(mpr.readPressure());
  Serial.print("pressureZero_kPa = ");
  Serial.println(pressureZero_kPa, 1);
  // ------------------------------------


  // ---- Step 2: known weight calibration ----
  Serial.println("\nStep 2: KNOWN WEIGHT");
  Serial.println("Place a known weight on the bellow and leave it there.");
  Serial.println("Now type that weight in GRAMS (e.g. 100) and press Enter:");

  calibrationWeight_g = 20.0f; // Default value

  Serial.print("Calibration weight = ");
  Serial.print(calibrationWeight_g, 2);
  Serial.println(" g");

  pressureCali_kPa = pressureZero_kPa + 0.4;  // HPA_TO_KPA(mpr.readPressure());
  Serial.print("pressureCali_kPa = ");
  Serial.println(pressureCali_kPa, 1);

  float pressureDelta_kPa = pressureCali_kPa - pressureZero_kPa;
  if (pressureDelta_kPa == 0.0f) {
    Serial.println("ERROR: pressureDelta_kPa is zero. Check sensor / wiring / weight.");
    forceToSensorRatio = 0.0f;
  } else {

    float F_N = GRAM_TO_NEWTON(calibrationWeight_g);
    forceToSensorRatio = F_N / pressureDelta_kPa;  // N/kPa

    Serial.println("\nCalibration complete.");
    Serial.print("forceToSensorRatio = ");
    Serial.print(forceToSensorRatio, 6);
    Serial.println(" N/kPa");
  }

  Serial.println("\nStarting live readings...\n");
}


void loop() {
  if (! mpr.begin()) {
    Serial.println("Failed to communicate with MPRLS sensor, check wiring?");
    delay(10);
    return;
  }

  float pressure_kPa = HPA_TO_KPA(mpr.readPressure());
  Serial.print("Pressure (kPa): "); Serial.println(pressure_kPa);
  Serial.print("Pressure (PSI): "); Serial.println(KPA_TO_PSI(pressure_kPa));

  // Gauge pressure relative to zero-load
  float F_N = (pressure_kPa - pressureZero_kPa) * forceToSensorRatio;
  float weight_g = NEWTON_TO_GRAM(F_N);

  // Serial Logging
  Serial.print("Detected weight: ");
  Serial.print(weight_g, 1);
  Serial.println(" g");

  delay(10);
}
