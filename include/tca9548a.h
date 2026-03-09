#pragma once

#include "config.h"

#include <Wire.h>
#include <Adafruit_I2CDevice.h>
#include <map>

#define DEFAULT_TCAADDR 0x70

#define TCA9548A_MIN_CHANNEL 0
#define TCA9548A_MAX_CHANNEL 7
#define TCA9548A_CHANNEL_COUNT 8


/**
 * Selects the active channel on a TCA9548A I2C multiplexer.
 *
 * Sends a one-hot bitmask to the device at the given I2C address to enable
 * the requested channel (0..7). Uses Wire.beginTransmission and
 * Wire.endTransmission to perform the I2C write.
 *
 * @param i     Channel index to enable (valid range: 0..7).
 * @param addr  I2C address of the TCA9548A (defaults to DEFAULT_TCAADDR).
 * @return      0 on success (no error), otherwise the error code returned by
 *              Wire.endTransmission(); returns -1 if the channel index is invalid.
 */
int tcaselect(uint8_t i, uint8_t addr = DEFAULT_TCAADDR) {
  if (i > TCA9548A_MAX_CHANNEL) return -1;
  Wire.beginTransmission(addr);
  Wire.write(1 << i);
  return Wire.endTransmission();
}

/**
 * @brief Disable all channels on a TCA9548A I2C multiplexer.
 *
 * Writes 0x00 to the TCA9548A control register to turn off (mask) all downstream channels.
 * This affects subsequent I2C communications routed through the multiplexer until channels
 * are re-enabled.
 *
 * @param addr I2C 7-bit address of the TCA9548A. Defaults to DEFAULT_TCAADDR.
 *             Ensure Wire.begin(SDA_PIN, SCL_PIN) has been called prior to invoking this function.
 *
 * @return int Status code returned by Wire.endTransmission():
 *             0 = Success
 *             1 = Data too long to fit in transmit buffer
 *             2 = Received NACK on transmit of address
 *             3 = Received NACK on transmit of data
 *             4 = Other error
 *             5 = Timeout
 *
 * @note This function performs a blocking I2C transaction. Use with care in timing-sensitive contexts.
 */
int tcadisable(uint8_t addr = DEFAULT_TCAADDR) {
  Wire.beginTransmission(addr);
  Wire.write(0x00); // disable all channels
  return Wire.endTransmission();
}

// Helper: count set bits in an 8-bit bitmask (used for TCA9548A port bitmask)
int sumBits(uint8_t bits) {
  int count = 0;
  while (bits) {
    count += bits & 1;
    bits >>= 1;
  }
  return count;
}

/**
 * @brief Scan TCA9548A multiplexer channels for a sensor and return a bitmask of responding channels.
 *
 * Probes each channel in the range [TCA9548A_MIN_CHANNEL, TCA9548A_MAX_CHANNEL] on the given
 * multiplexer I2C address by selecting the channel (tcaselect) and attempting an I2C transmission
 * to sensor_addr. If the probe succeeds (Wire.endTransmission() == 0) the corresponding
 * bit for that channel is set in the returned mask.
 *
 * Side effects:
 *  - Calls tcaselect(channel, mux) for each channel; non-zero return values are logged to Serial
 *    ("TCASELECT Error Code <res> found at channel <i> on mux 0x<mux>") and that channel is skipped.
 *  - Uses Wire.beginTransmission/Wire.endTransmission to probe the device address.
 *  - Calls tcadisable(mux) at the end to disable the multiplexer.
 *  - Alters the I2C bus state while probing.
 *
 * @param sensor_addr I2C address of the sensor to probe for (e.g. MPRLS_ADDR).
 * @param mux I2C address of the TCA9548A multiplexer to scan (default: DEFAULT_TCAADDR).
 * @return uint8_t Bitmask of found ports; bit i (1 << i) is set if a device responded on channel i.
 *         If no devices are found the function returns 0x00.
 *
 * Notes:
 *  - Assumes channel indices fit in the return byte (typical TCA9548A channels 0..7).
 *  - Relies on the constants TCA9548A_MIN_CHANNEL, TCA9548A_MAX_CHANNEL, etc.
 *  - Not reentrant/thread-safe.
 */
uint8_t tcaselectValidPorts(uint8_t sensor_addr, uint8_t mux = DEFAULT_TCAADDR) {
  uint8_t found_ports = 0x00;
  for (uint8_t i = TCA9548A_MIN_CHANNEL; i <= TCA9548A_MAX_CHANNEL; i++) {
    int res = tcaselect(i, mux);
    // TODO: consider logging after finishing all channels instead of per-channel
    if (res != 0) {
      Serial.print("TCASELECT Error Code ");
      Serial.print(res);
      Serial.print(" found at channel ");
      Serial.print(i);
      Serial.print(" on mux 0x");
      Serial.println(mux, HEX);
      continue; // no mux/device there
    }

    Wire.beginTransmission(sensor_addr);
    if (!Wire.endTransmission()) {
      found_ports |= (1 << i);
    }
  }
  tcadisable(mux);
  return found_ports;
}

// Scan multiple multiplexers and channels for sensors and return a mapping of mux addresses to bitmasks of valid channels
void scanAvailableSensorOverMultipleTCAs(uint8_t sensor_addr, std::map<uint8_t, uint8_t>& mux_to_valid_channels_mask) {
  for (const auto& mux : TCAADDR_ADDRESSES) {
    tcadisable(mux);  // Ensure mux is disabled before scanning
  }
  for (const auto& mux : TCAADDR_ADDRESSES) {
    uint8_t valid_channels_mask = tcaselectValidPorts(sensor_addr, mux);
    if (valid_channels_mask != 0x00) {
      Serial.print("Found Sensor on TCA9548A 0x");
      Serial.print(mux, HEX);
      Serial.print(" ports bitmask: 0x");
      Serial.println(valid_channels_mask, BIN);
      int num_active_ports = sumBits(valid_channels_mask);
      Serial.print("Number of active ports with Sensor: ");
      Serial.println(num_active_ports);
      mux_to_valid_channels_mask[mux] = valid_channels_mask;
    }
  }
}
