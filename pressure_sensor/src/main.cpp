#include "config.h"
#include "sensor_utils.h"
#include "pneumatic_load_cell.hpp"
#include "serial_bridge.hpp"

SerialBridge bridge = SerialBridge();

// Dummy MPRLS object for probing devices during setup
Adafruit_MPRLS test_mpr = Adafruit_MPRLS(RESET_PIN, EOC_PIN, 0, 25, 10, 90, PSI_to_KPA);

// Load cell objects and mapping of mux addresses to valid channel bitmasks will be populated in setup() after scanning for devices;
std::map<uint8_t, uint8_t> mux_to_valid_channels_mask; // Map of mux address to bitmask of valid channels
std::array<std::unique_ptr<PneumaticLoadCell>, NUM_OF_SENSOR_SLOTS> load_cells; // List of load cells corresponding to detected sensors

void setup() {
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(I2C_CLOCK_FREQ);
  Serial.begin(BRIDGE_BAUDRATE);
  bridge.begin(Serial);

  Serial.println("MPRLS Load Cell Test");
  Serial.println("--------------------------------");

  scanAvailableSensorOverMultipleTCAs(mux_to_valid_channels_mask);
  for (const auto& entry : mux_to_valid_channels_mask) {
    const uint8_t& mux_addr = entry.first;
    const uint8_t& channels_mask = entry.second;
    const int mux_idx = static_cast<int>(mux_addr - DEFAULT_TCAADDR);

    for (uint8_t m = channels_mask; m; m &= (m - 1)) {
      const uint8_t lsb = static_cast<uint8_t>(m & -m);   // isolate lowest set bit
      const int ch = __builtin_ctz(lsb);                  // ESP32/GCC: index of that bit (0..7)
      const int sensor_idx = mux_idx * 8 + ch;            // calculate the unique sensor index

      Serial.print("\n--- Selecting TCA9548A 0x");
      Serial.print(mux_addr, HEX);
      Serial.print(" port ");
      Serial.print(ch);
      Serial.println(" ---");
      tcaselect(ch, mux_addr);
      if (!test_mpr.begin(MPRLS_ADDR)) {
        Serial.println("Failed to communicate with MPRLS sensor, check wiring? Please reboot after fixing.");
        tcadisable(mux_addr);
        while (1) {
          delay(READING_TIMEOUT);
        }
      }
      Serial.println("Found MPRLS sensor");
      load_cells[sensor_idx] = std::move(std::unique_ptr<PneumaticLoadCell>(new PneumaticLoadCell(ch, mux_addr)));

      // Zero-load calibration
      if (load_cells[sensor_idx]) {
        load_cells[sensor_idx]->begin();
        load_cells[sensor_idx]->resetZeroLoad();
        Serial.println("Zero load reset complete.");
      } else {
        Serial.println("Critical Error: Load cell pointer is null!");
      }
    }
    tcadisable(mux_addr);
  }

  Serial.println("\nStarting live readings...\n");
}

unsigned long lastMillis = 0;
void loop() {
  // Check for bridge updates (e.g. incoming control packets)
  // if (bridge.update()) {

  // }

  // Read sensors at defined sampling rate
  if (millis() - lastMillis >= MPRLS_SAMPLING_INTERVAL_MS) {
    lastMillis = millis();

    for (const auto& entry : mux_to_valid_channels_mask) {
      const uint8_t& mux_addr = entry.first;
      const uint8_t& channels_mask = entry.second;
      const int mux_idx = static_cast<int>(mux_addr - DEFAULT_TCAADDR);

      for (uint8_t m = channels_mask; m; m &= (m - 1)) {
        const uint8_t lsb = static_cast<uint8_t>(m & -m);   // isolate lowest set bit
        const int ch = __builtin_ctz(lsb);                  // ESP32/GCC: index of that bit (0..7)
        const int sensor_idx = mux_idx * 8 + ch;            // calculate the unique sensor index

        Serial.print("\n--- Selecting TCA9548A 0x");
        Serial.print(mux_addr, HEX);
        Serial.print(" port ");
        Serial.print(ch);
        Serial.println(" ---");
        if (!load_cells[sensor_idx]->begin()) { continue; }

        // Read data
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

        // Send via SerialBridge
        SensorPacket pkt;
        pkt.sensor_idx = static_cast<uint8_t>(sensor_idx);
        pkt.sensor_pressure_kPa = pressure_kPa;
        pkt.sensor_pressure_rate_kPa_s = pressure_rate;
        pkt.sensor_force_g = F_g;

        bridge.sendSensorPacket(pkt);
      }
      tcadisable(mux_addr);
    }

    unsigned long loop_time = millis() - lastMillis;
    if (loop_time > MPRLS_SAMPLING_INTERVAL_MS) {
      Serial.print("Warning: Loop time ");
      Serial.print(loop_time);
      Serial.println("ms exceeds sampling interval!");
    }
  }
}
