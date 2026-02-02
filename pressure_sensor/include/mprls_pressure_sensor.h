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


uint8_t tcaselect_valid_ports(uint8_t mux = DEFAULT_TCAADDR) {
  uint8_t found_ports = 0x00;
  for (uint8_t i = TCA9548A_MIN_CHANNEL; i <= TCA9548A_MAX_CHANNEL; i++) {
    int res = tcaselect(i, mux);
    if (res != 0) return found_ports; // no mux/device there

    for (uint8_t addr = I2C_MIN_ADDRESS; addr <= I2C_MAX_ADDRESS; addr++) {
      if (addr == mux) continue;

      Wire.beginTransmission(addr);
      if (!Wire.endTransmission() && addr == MPRLS_DEFAULT_ADDR) {
        found_ports |= (1 << i);
      }
    }
  }
  tcadisable(mux);
  return found_ports;
}
