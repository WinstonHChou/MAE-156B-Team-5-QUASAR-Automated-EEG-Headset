#include "config.h"
#include "sensor_utils.h"

#define MUX_ADDR DEFAULT_TCAADDR

// ---- Sensor object ----
Adafruit_MPRLS test_mpr = Adafruit_MPRLS(RESET_PIN, EOC_PIN, 0, 25, 10, 90, PSI_to_KPA);

// ---- Calibration variables ----
std::map<uint8_t, std::unique_ptr<PneumaticLoadCell>> load_cells; // List of load cells corresponding to detected sensors

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
    if (!test_mpr.begin(MPRLS_ADDR)) {
      Serial.println("Failed to communicate with MPRLS sensor, check wiring? Please reboot after fixing.");
      while (1) {
        delay(READING_TIMEOUT);
      }
    }
    Serial.println("Found MPRLS sensor");
    load_cells[ch] = std::move(std::unique_ptr<PneumaticLoadCell>(new PneumaticLoadCell(ch, MUX_ADDR)));

    // Zero-load calibration
    if (load_cells[ch]) {
      load_cells[ch]->begin();
      load_cells[ch]->resetZeroLoad();
      Serial.println("Zero load reset complete.");
    } else {
      Serial.println("Critical Error: Load cell pointer is null!");
    }
  }
  tcadisable(MUX_ADDR);

  Serial.println("\nStarting live readings...\n");
}

unsigned long lastMillis = 0;
void loop() {
  if (millis() - lastMillis >= MPRLS_SAMPLING_INTERVAL_MS) {
    lastMillis = millis();

    for (uint8_t m = found_ports; m; m &= (m - 1)) {
      uint8_t lsb = m & -m;                 // isolate lowest set bit
      uint8_t ch  = __builtin_ctz(lsb);     // ESP32/GCC: index of that bit (0..7)

      Serial.print("\n--- Selecting TCA9548A port ");
      Serial.print(ch);
      Serial.println(" ---");
      if (!load_cells[ch]->begin()) { continue; }

      // Read pressure in kPa and force in grams
      float pressure_kPa = load_cells[ch]->readPressure();
      float F_g = load_cells[ch]->getForceFromPressure();
      float pressure_rate = load_cells[ch]->getPressureRate();

      // Serial Logging
      Serial.print(">");
      Serial.print("Pressure_kPa_"); Serial.print(ch); Serial.print(":"); Serial.print(pressure_kPa, 4);
      Serial.print(",Pressure_PSI_"); Serial.print(ch); Serial.print(":"); Serial.print(KPA_TO_PSI(pressure_kPa), 4);
      Serial.print(",Detected_weight_g_"); Serial.print(ch); Serial.print(":"); Serial.print(F_g, 4);
      Serial.print(",Pressure_rate_kPa_s_"); Serial.print(ch); Serial.print(":"); Serial.print(pressure_rate, 4);
      Serial.println();
    }
  }
}
