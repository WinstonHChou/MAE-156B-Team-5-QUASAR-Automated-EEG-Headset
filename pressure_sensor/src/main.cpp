#include "mprls_pressure_sensor.h"

#define MUX_ADDR DEFAULT_TCAADDR

// ---- Sensor object ----
Adafruit_MPRLS mpr = Adafruit_MPRLS(RESET_PIN, EOC_PIN);

// // ---- Calibration variables ----
float pressureZero_kPa        = 0.0f;             // raw counts at zero weight
// float pressureCali_kPa        = 0.0f;             // raw counts at known weight
// float forceToSensorRatio      = 0.0f;             // slope: Pa per ADC count
// float calibrationWeight_g     = 0.0f;             // known mass in grams

uint8_t found_ports;

// Wait for user to hit Enter in Serial Monitor
void waitForEnter() {
  while (!Serial.available()) { }
  while (Serial.available()) { Serial.read(); } // clear buffer
}

void setup() {
  Wire.begin();

  Serial.begin(115200);
  Serial.println("MPRLS Load Cell Test");
  Serial.println("--------------------------------");

  found_ports = tcaselectValidPorts(MUX_ADDR);
  Serial.print("Found MPRLS on TCA9548A ports bitmask: 0x");
  Serial.println(found_ports, BIN);
  int num_active_ports = sumBits(found_ports);
  Serial.print("Number of active ports with MPRLS: ");
  Serial.println(num_active_ports);

  for (uint8_t m = found_ports; m; m &= (m - 1)) {
    uint8_t lsb = m & -m;                 // isolate lowest set bit
    uint8_t ch  = __builtin_ctz(lsb);     // ESP32/GCC: index of that bit (0..7)

    Serial.print("\n--- Selecting TCA9548A port ");
    Serial.print(ch);
    Serial.println(" ---");
    tcaselect(ch, MUX_ADDR);
    if (!mpr.begin()) {
      Serial.println("Failed to communicate with MPRLS sensor, check wiring?");
      while (1) {
        delay(10);
      }
    }
    Serial.println("Found MPRLS sensor");

    // TODO: differentiate multiple sensors calibration on different ports
    // ---- Step 1: zero-load baseline ----
    Serial.println("Step 1: ZERO LOAD");
    Serial.println("Make sure there is NO weight on the bellow.");
    Serial.println("Press Enter in the Serial Monitor when ready.");
    // waitForEnter();

    pressureZero_kPa = HPA_TO_KPA(mpr.readPressure());
    // Serial.print("pressureZero_kPa = ");
    // Serial.println(pressureZero_kPa, 1);
    // ------------------------------------


  //   // ---- Step 2: known weight calibration ----
  //   Serial.println("\nStep 2: KNOWN WEIGHT");
  //   Serial.println("Place a known weight on the bellow and leave it there.");
  //   Serial.println("Now type that weight in GRAMS (e.g. 100) and press Enter:");

  //   calibrationWeight_g = 20.0f; // Default value

  //   Serial.print("Calibration weight = ");
  //   Serial.print(calibrationWeight_g, 2);
  //   Serial.println(" g");

  //   pressureCali_kPa = pressureZero_kPa + 0.4;  // HPA_TO_KPA(mpr.readPressure());
  //   Serial.print("pressureCali_kPa = ");
  //   Serial.println(pressureCali_kPa, 1);

  //   float pressureDelta_kPa = pressureCali_kPa - pressureZero_kPa;
  //   if (pressureDelta_kPa == 0.0f) {
  //     Serial.println("ERROR: pressureDelta_kPa is zero. Check sensor / wiring / weight.");
  //     forceToSensorRatio = 0.0f;
  //   } else {

  //     float F_N = GRAM_TO_NEWTON(calibrationWeight_g);
  //     forceToSensorRatio = F_N / pressureDelta_kPa;  // N/kPa

  //     Serial.println("\nCalibration complete.");
  //     Serial.print("forceToSensorRatio = ");
  //     Serial.print(forceToSensorRatio, 6);
  //     Serial.println(" N/kPa");
  //   }
  }
  tcadisable(MUX_ADDR);

  Serial.println("\nStarting live readings...\n");
}


void loop() {
  for (uint8_t m = found_ports; m; m &= (m - 1)) {
    uint8_t lsb = m & -m;                 // isolate lowest set bit
    uint8_t ch  = __builtin_ctz(lsb);     // ESP32/GCC: index of that bit (0..7)

    Serial.print("\n--- Selecting TCA9548A port ");
    Serial.print(ch);
    Serial.println(" ---");
    tcaselect(ch, MUX_ADDR);
    if (! mpr.begin()) {
      Serial.println("Failed to communicate with MPRLS sensor, check wiring?");
      delay(10);
      return;
    }

    // Serial message starts
    Serial.print(">");
  
    float pressure_kPa = HPA_TO_KPA(mpr.readPressure());
    Serial.print("Pressure_kPa:"); Serial.print(pressure_kPa);
    Serial.print(",Pressure_PSI:"); Serial.print(KPA_TO_PSI(pressure_kPa));

    // Gauge pressure relative to zero-load
    float F_g = (pressure_kPa - pressureZero_kPa) * FORCE_TO_SENSOR_RATIO;

    // Serial Logging
    Serial.print(",Detected_weight_g:");
    Serial.print(F_g, 1);
    Serial.println();

    delay(10);
  }
}
