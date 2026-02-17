#include <Wire.h>
#include <Adafruit_I2CDevice.h>

#define DEFAULT_TCAADDR 0x70

#define TCA9548A_MIN_CHANNEL 0
#define TCA9548A_MAX_CHANNEL 7
#define TCA9548A_CHANNEL_COUNT 8

#define I2C_MIN_ADDRESS 0x00
#define I2C_MAX_ADDRESS 0x7F


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
 *             Ensure Wire.begin() has been called prior to invoking this function.
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
