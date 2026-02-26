#include "config.h"
#include "tca9548a.h"

#include <memory>
#include <vector>
#include <map>
#include <Adafruit_MPRLS.h>
#include <Iir.h>

// Physical constants
#define GRAVITY 9.80665f                              // m/s^2
#define KPA_TO_PSI(x) ((x) / 6.8947572932f)           // 1 PSI = 6.8947572932 kPa
#define HPA_TO_KPA(x) ((x) / 10.0f)                   // 1 hPa = 0.1 kPa
#define GRAM_TO_NEWTON(x) ((x) / 1000.0f * GRAVITY)   // convert grams to Newtons
#define NEWTON_TO_GRAM(x) ((x) * 1000.0f / GRAVITY) // convert Newtons to grams
#define PA_TO_KPA(x) ((x) / 1000.0f)                  // 1 Pa = 0.001 kPa
#define PSI_to_KPA (6.8947572932f)   ///< Constant: PSI to KPA conversion factor


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
 * to MPRLS_ADDR. If the probe succeeds (Wire.endTransmission() == 0) the corresponding
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
 *  - Relies on the constants TCA9548A_MIN_CHANNEL, TCA9548A_MAX_CHANNEL, and MPRLS_ADDR.
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

    Wire.beginTransmission(MPRLS_ADDR);
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

// Pneumatic Load Cell
class PneumaticLoadCell {
  public:
    PneumaticLoadCell(uint8_t channel, uint8_t mux_addr)
      : ch_(channel), mux_(mux_addr) {
      lp_.setup(MPRLS_SAMPLING_RATE_HZ, LOWPASS_CUTOFF_FREQ_HZ);
    }

    // Read pressure from the sensor
    boolean begin() {
      tcaselect(ch_, mux_);
      if (!mpr_.begin(MPRLS_ADDR)) {
        Serial.println("Failed to communicate with MPRLS sensor, check wiring?");
        delay(READING_TIMEOUT);
        return false;
      }
      return true;
    }

    float readPressure() {
      prev_kPa_ = current_kPa_;
      current_kPa_ = lp_.filter(mpr_.readPressure());

      // Estimator pipeline: only update force reading if pressure rate is above threshold to filter out drifts;
      // otherwise calculate drifting compensated zero load pressure
      if (abs(getPressureRate()) > MIN_ACCEPTABLE_PRESSURE_RATE_THRESHOLD_KPA_S) {
        // Update force reading only if pressure rate is above threshold to filter out drifts
        current_force_g_ = (current_kPa_ - zero_kPa_) * ratio_;  // in grams
      } else {
        // Calculate drifting compensated zero load pressure
        zero_kPa_ = current_kPa_ - (current_force_g_ / ratio_);
      }
      return current_kPa_;
    }

    float getPressureRate() {
      return current_kPa_ - prev_kPa_;  // simple finite difference; could be improved with more history
    }

    float getForceFromPressure() {
      return current_force_g_;
    }

    // Calibration procedure
    void resetZeroLoad() {
      zero_kPa_ = mpr_.readPressure();
    }

  private:
    Adafruit_MPRLS mpr_ = Adafruit_MPRLS(RESET_PIN, EOC_PIN,
                                         0, 25,
                                         10, 90,
                                         PSI_to_KPA);
    Iir::Butterworth::LowPass<2> lp_;
    uint8_t ch_;           // Channel number
    uint8_t mux_;          // Multiplexer address

    float prev_kPa_ = 0.0;    // Previous pressure reading (kPa)
    float current_kPa_ = 0.0; // Latest pressure reading (kPa)
    float zero_kPa_ = 0.0; // Pressure at zero load (kPa)
    float ratio_ = FORCE_TO_SENSOR_RATIO;    // Force-to-sensor ratio (grams/kPa)
};