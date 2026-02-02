#include <Adafruit_MPRLS.h>
#include "tca9548a.h"

// You dont *need* a reset and EOC pin for most uses, so we set to -1 and don't connect
#define RESET_PIN  -1  // set to any GPIO pin # to hard-reset on begin()
#define EOC_PIN    -1  // set to any GPIO pin to read end-of-conversion by pin

// future multi-mux support:
// for (uint8_t mux = 0x70; mux <= 0x77; ++mux) {
//   for (uint8_t ch = 0; ch < 8; ++ch) {
//     if (tcaselect(ch, mux) != 0) continue; // no mux/device there
//     // probe downstream addresses or talk to devices
//     // ...
//     tcadisable(mux);
//   }
// }


/**
 * @brief Scan TCA9548A multiplexer channels for an MPRLS device and return a bitmask of responding channels.
 *
 * Probes each channel in the range [TCA9548A_MIN_CHANNEL, TCA9548A_MAX_CHANNEL] on the given
 * multiplexer I2C address by selecting the channel (tcaselect) and attempting an I2C transmission
 * to MPRLS_DEFAULT_ADDR. If the probe succeeds (Wire.endTransmission() == 0) the corresponding
 * bit for that channel is set in the returned mask.
 *
 * Side effects:
 *  - Calls tcaselect(channel, mux) for each channel; non-zero return values are logged to Serial
 *    ("TCASELECT Error Code <res> found at channel <i> on mux 0x<mux>") and that channel is skipped.
 *  - Uses Wire.beginTransmission/Wire.endTransmission to probe the device address.
 *  - Calls tcadisable(mux) at the end to disable the multiplexer.
 *  - Alters the I2C bus state while probing.
 *
 * @param mux I2C address of the TCA9548A multiplexer to scan (default: DEFAULT_TCAADDR).
 * @return uint8_t Bitmask of found ports; bit i (1 << i) is set if a device responded on channel i.
 *         If no devices are found the function returns 0x00.
 *
 * Notes:
 *  - Assumes channel indices fit in the return byte (typical TCA9548A channels 0..7).
 *  - Relies on the constants TCA9548A_MIN_CHANNEL, TCA9548A_MAX_CHANNEL, and MPRLS_DEFAULT_ADDR.
 *  - Not reentrant/thread-safe.
 */
uint8_t tcaselectValidPorts(uint8_t mux = DEFAULT_TCAADDR) {
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

    Wire.beginTransmission(MPRLS_DEFAULT_ADDR);
    if (!Wire.endTransmission()) {
      found_ports |= (1 << i);
    }
  }
  tcadisable(mux);
  return found_ports;
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
