#include "config.h"
#include "sensor_utils.h"
#include "pneumatic_load_cell.hpp"

// Dummy MPRLS object for probing devices during setup
Adafruit_MPRLS test_mpr = Adafruit_MPRLS(RESET_PIN, EOC_PIN, 0, 25, 10, 90, PSI_to_KPA);

// Load cell objects and mapping of mux addresses to valid channel bitmasks will be populated in setup() after scanning for devices;
std::map<uint8_t, uint8_t> mux_to_valid_channels_mask; // Map of mux address to bitmask of valid channels
std::array<std::unique_ptr<PneumaticLoadCell>, NUM_OF_SENSOR_SLOTS> load_cells; // List of load cells corresponding to detected sensors

void setup() {
  Wire.begin(SDA_PIN, SCL_PIN);

  Serial.begin(115200);
  Serial.println("MPRLS Load Cell Test");
  Serial.println("--------------------------------");

  scanAvailableSensorOverMultipleTCAs(mux_to_valid_channels_mask);
  for (const auto& entry : mux_to_valid_channels_mask) {
    int mux_idx = static_cast<int>(entry.first - DEFAULT_TCAADDR);
    for (uint8_t m = entry.second; m; m &= (m - 1)) {
      uint8_t lsb = m & -m;                       // isolate lowest set bit
      int ch = __builtin_ctz(lsb);    // ESP32/GCC: index of that bit (0..7)

      Serial.print("\n--- Selecting TCA9548A 0x");
      Serial.print(entry.first, HEX);
      Serial.print(" port ");
      Serial.print(ch);
      Serial.println(" ---");
      tcaselect(ch, entry.first);
      if (!test_mpr.begin(MPRLS_ADDR)) {
        Serial.println("Failed to communicate with MPRLS sensor, check wiring? Please reboot after fixing.");
        tcadisable(entry.first);
        while (1) {
          delay(READING_TIMEOUT);
        }
      }
      Serial.println("Found MPRLS sensor");
      int sensor_idx = mux_idx * 8 + ch; // calculate a unique sensor index based on mux address and channel
      load_cells[sensor_idx] = std::move(std::unique_ptr<PneumaticLoadCell>(new PneumaticLoadCell(ch, entry.first)));

      // Zero-load calibration
      if (load_cells[sensor_idx]) {
        load_cells[sensor_idx]->begin();
        load_cells[sensor_idx]->resetZeroLoad();
        Serial.println("Zero load reset complete.");
      } else {
        Serial.println("Critical Error: Load cell pointer is null!");
      }
    }
    tcadisable(entry.first);
  }

  Serial.println("\nStarting live readings...\n");
}

unsigned long lastMillis = 0;
void loop() {
  if (millis() - lastMillis >= MPRLS_SAMPLING_INTERVAL_MS) {
    lastMillis = millis();

    for (const auto& entry : mux_to_valid_channels_mask) {
      int mux_idx = static_cast<int>(entry.first - DEFAULT_TCAADDR);
      for (uint8_t m = entry.second; m; m &= (m - 1)) {
        uint8_t lsb = m & -m;             // isolate lowest set bit
        int ch = __builtin_ctz(lsb);      // ESP32/GCC: index of that bit (0..7)

        int sensor_idx = mux_idx * 8 + ch; // calculate the unique sensor index
        Serial.print("\n--- Selecting TCA9548A 0x");
        Serial.print(entry.first, HEX);
        Serial.print(" port ");
        Serial.print(ch);
        Serial.println(" ---");
        if (!load_cells[sensor_idx]->begin()) { continue; }

        // Read pressure in kPa and force in grams
        float pressure_kPa = load_cells[sensor_idx]->readPressure();
        float F_g = load_cells[sensor_idx]->getForceFromPressure();
        float pressure_rate = load_cells[sensor_idx]->getPressureRate();

        // Serial Logging
        Serial.print(">");
        Serial.print("Pressure_kPa_"); Serial.print(sensor_idx); Serial.print(":"); Serial.print(pressure_kPa, 4);
        Serial.print(",Pressure_PSI_"); Serial.print(sensor_idx); Serial.print(":"); Serial.print(pressure_kPa / PSI_to_KPA, 4);
        Serial.print(",Detected_weight_g_"); Serial.print(sensor_idx); Serial.print(":"); Serial.print(F_g, 4);
        Serial.print(",Pressure_rate_kPa_s_"); Serial.print(sensor_idx); Serial.print(":"); Serial.print(pressure_rate, 4);
        Serial.println();
      }
      tcadisable(entry.first);
    }

  }
}
