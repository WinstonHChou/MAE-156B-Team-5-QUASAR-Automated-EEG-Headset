#pragma once

#include "config.h"
#include "tca9548a.h"

#include <map>
#include <iterator>
#include <limits>
#include <Adafruit_MPRLS.h>
#include <Iir.h>

#define MPRLS_ADDR MPRLS_DEFAULT_ADDR
#define PSI_to_KPA (6.8947572932f)   ///< Constant: PSI to KPA conversion factor

double interp_clamp(const std::map<double,double>& m, double x) {
  if (m.empty()) return std::numeric_limits<double>::quiet_NaN();

  auto hi = m.lower_bound(x);                 // first key >= x

  if (hi == m.begin()) return hi->second;     // x <= first key (clamp)
  if (hi == m.end())   return std::prev(hi)->second; // x > last key (clamp)

  auto lo = std::prev(hi);                    // key < x

  const double x0 = lo->first, y0 = lo->second;
  const double x1 = hi->first, y1 = hi->second;

  const double t = (x - x0) / (x1 - x0);
  return y0 + t * (y1 - y0);
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

    // void calibrateForceToSensorRatio(float known_force_grams) {
    //   float pressure_kPa = mpr_.readPressure();
    //   if (pressure_kPa > zero_kPa_) {
    //     ratio_ = known_force_grams / (pressure_kPa - zero_kPa_);
    //   }
    // }

  private:
    Adafruit_MPRLS mpr_ = Adafruit_MPRLS(RESET_PIN, EOC_PIN,
                                         0, 25,
                                         10, 90,
                                         PSI_to_KPA);
    Iir::Butterworth::LowPass<2> lp_;
    uint8_t ch_;           // Channel number
    uint8_t mux_;          // Multiplexer address
    // std::map<double, double> calibration_map_; // Map of <pressure (kPa), force (grams)>

    float prev_kPa_ = 0.0;    // Previous pressure reading (kPa)
    float current_kPa_ = 0.0; // Latest pressure reading (kPa)
    float current_force_g_ = 0.0; // Latest force reading (grams)
    float zero_kPa_ = 0.0; // Pressure at zero load (kPa)
    float ratio_ = FORCE_TO_SENSOR_RATIO;    // Force-to-sensor ratio (grams/kPa)
};
