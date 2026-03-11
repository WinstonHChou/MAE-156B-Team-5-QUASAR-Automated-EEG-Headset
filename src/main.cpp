#include "config.h"
#include "tca9548a.h"
#include "pneumatic_load_cell.hpp"
#include "serial_bridge.hpp"

#include <memory>
#include <array>


SerialBridge bridge = SerialBridge();

// Load cell objects and mapping of mux addresses to valid channel bitmasks will be populated in setup() after scanning for devices;
std::array<std::unique_ptr<PneumaticLoadCell>, NUM_OF_SENSOR_SLOTS> load_cells; // List of load cells corresponding to detected sensors

void setup() {
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(I2C_CLOCK_FREQ);
  Serial.begin(BRIDGE_BAUDRATE);
  bridge.begin(Serial);

  Serial.println("MPRLS Load Cell Test");
  Serial.println("--------------------------------");

  std::map<uint8_t, uint8_t> mux_to_valid_channels_mask; // Map of mux address to bitmask of valid channels
  scanAvailableSensorOverMultipleTCAs(MPRLS_ADDR, mux_to_valid_channels_mask);
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

      std::unique_ptr<PneumaticLoadCell> sensor = std::unique_ptr<PneumaticLoadCell>(new PneumaticLoadCell(ch, mux_addr, sensor_idx));
      if (sensor->begin()) {
        // Zero-load calibration
        sensor->resetZeroLoad();
        Serial.println("Zero load reset complete.");

        // Store the sensor object in the load_cells array at the index corresponding to its unique sensor index
        load_cells[sensor_idx] = std::move(sensor);
        Serial.println("Sensor initialized.");
      }
    }
    tcadisable(mux_addr);
  }

  Serial.println("\nStarting live readings...\n");
}

unsigned long lastMillis = 0;
uint8_t prev_mux_addr = TCAADDR_ADDRESSES[0];
void loop() {
  // Check for bridge requests from host, which are sent as ControlPackets.
  ControlPacket pkt = {};
  if (bridge.receive(pkt) && pkt.sensor_idx < NUM_OF_SENSOR_SLOTS && load_cells[pkt.sensor_idx]) {

    switch (pkt.request_idx) {
      case REQUEST_TARING:
        if (load_cells[pkt.sensor_idx]->getStatus() == PneumaticLoadCell::OK) {
          load_cells[pkt.sensor_idx]->resetZeroLoad();
        } else {
          pkt.flags |= CTRL_ERR; // Cannot perform zero load reset if sensor is not in OK status
          pkt.error_code = ERR_INVALID_REQUEST;
        }
        break;
      case REQUEST_CALIBRATION_START:
        load_cells[pkt.sensor_idx]->setToCalibrationMode();
        break;
      case REQUEST_CALIBRATION_END:
        load_cells[pkt.sensor_idx]->setRatio(static_cast<float>(pkt.payload));  // End calibration mode to save the new ratio
        load_cells[pkt.sensor_idx]->resetZeroLoad();                            // After calibration, reset zero load to update the reference
        load_cells[pkt.sensor_idx]->setToNormalMode();
        break;
      case REQUEST_HARDWARE_RESET:
        load_cells[pkt.sensor_idx]->resetHardware();
      break;
      default:
        pkt.flags |= CTRL_ERR; // Invalid request type
        pkt.error_code = ERR_INVALID_REQUEST;
        break;
    }

    pkt.flags |= CTRL_ACK; // Acknowledge receipt of the control packet
    switch (load_cells[pkt.sensor_idx]->getStatus()) {
      case PneumaticLoadCell::OK:
        pkt.flags |= 0; // no additional flags
        break;
      case PneumaticLoadCell::BUSY:
        pkt.flags |= CTRL_BUSY;
        break;
      case PneumaticLoadCell::FAILURE:
        pkt.flags |= CTRL_ERR;
        pkt.error_code = ERR_SENSOR_FAILURE;
        break;
      default:
        break;
    }

    bridge.send(pkt); // Echo back the received control packet for confirmation
  }

  // Read sensors at defined sampling rate
  if (millis() - lastMillis >= MPRLS_SAMPLING_INTERVAL_MS) {
    lastMillis = millis();

    // STEP 1: Broadcast "Start" to all sensors
    for (auto& sensor : load_cells) {
      if (!sensor) continue; // Skip if sensor is not initialized

      if (sensor->getMuxAddress() != prev_mux_addr) {
        tcadisable(prev_mux_addr);
        prev_mux_addr = sensor->getMuxAddress();
      }
      sensor->requestMeasurement();
    }

    // STEP 2: Wait once for the longest conversion time (typically 5ms)
    // During this time, every sensor is busy-calculating pressure.
    delay(WAIT_FOR_CONVERSION_TIME_MS);

    // STEP 3: Read ambient pressure from the designated sensor for drift compensation
    if (AMBIENT_PRESSURE_SENSOR_IDX < NUM_OF_SENSOR_SLOTS && load_cells[AMBIENT_PRESSURE_SENSOR_IDX]) {
      if (load_cells[AMBIENT_PRESSURE_SENSOR_IDX]->getMuxAddress() != prev_mux_addr) {
        tcadisable(prev_mux_addr);
        prev_mux_addr = load_cells[AMBIENT_PRESSURE_SENSOR_IDX]->getMuxAddress();
      }
      load_cells[AMBIENT_PRESSURE_SENSOR_IDX]->update(); // Update to get the latest reading
      float ambient_kPa = load_cells[AMBIENT_PRESSURE_SENSOR_IDX]->getPressure();
      PneumaticLoadCell::updateAmbientPressure(ambient_kPa); // Update ambient pressure for drift compensation
    } else {
      // If ambient pressure sensor is not available, use DEFAULT_AMBIENT_PRESSURE_KPA
      PneumaticLoadCell::updateAmbientPressure(DEFAULT_AMBIENT_PRESSURE_KPA);
    }

    // STEP 4: Collect data and send via SerialTransfer
    for (auto& sensor : load_cells) {
      if (!sensor) continue; // Skip if sensor is not initialized
  
      // Skip sending data for ambient pressure sensor, it's only used for drift compensation
      if (sensor->getSensorIndex() == AMBIENT_PRESSURE_SENSOR_IDX) continue;

      if (sensor->getMuxAddress() != prev_mux_addr) {
        tcadisable(prev_mux_addr);
        prev_mux_addr = sensor->getMuxAddress();
      }
      // periodic update of sensor readings;
      sensor->update();

      // Read data
      float pressure_kPa = sensor->getPressure();
      float F_g = sensor->getForceFromPressure();
      float pressure_rate = sensor->getPressureRate();

      // Debug Serial Logging
      #ifdef DEBUG_SERIAL
      Serial.println();
      Serial.print(">");
      Serial.print("Pressure_kPa_"); Serial.print(sensor->getSensorIndex()); Serial.print(":"); Serial.print(pressure_kPa, 4);
      Serial.print(",Pressure_PSI_"); Serial.print(sensor->getSensorIndex()); Serial.print(":"); Serial.print(pressure_kPa / PSI_to_KPA, 4);
      Serial.print(",Detected_weight_g_"); Serial.print(sensor->getSensorIndex()); Serial.print(":"); Serial.print(F_g, 4);
      Serial.print(",Pressure_rate_kPa_s_"); Serial.print(sensor->getSensorIndex()); Serial.print(":"); Serial.print(pressure_rate, 4);
      Serial.println();
      #endif

      // Send via SerialBridge
      SensorPacket pkt = {};
      pkt.sensor_idx = sensor->getSensorIndex();
      pkt.sensor_pressure_kPa = pressure_kPa;
      pkt.sensor_pressure_rate_kPa_s = pressure_rate;
      pkt.sensor_force_g = F_g;

      bridge.send(pkt);
    }

    unsigned long loop_time = millis() - lastMillis;
    if (loop_time > MPRLS_SAMPLING_INTERVAL_MS) {
      #ifdef DEBUG_SERIAL
      Serial.println();
      Serial.print("Warning: Loop time ");
      Serial.print(loop_time);
      Serial.println("ms exceeds sampling interval!");
      #endif
      WatchdogPacket pkt = {};
      pkt.overrun = 1;
      pkt.loop_time_ms = loop_time;
      bridge.send(pkt);
    }
  }
}
