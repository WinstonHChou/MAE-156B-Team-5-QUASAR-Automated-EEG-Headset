#pragma once

#include "config.h"
#include "tca9548a.h"
#include "mprls0025pa00001a/mprls0025pa00001a.hpp"

#include <map>
#include <iterator>
#include <limits>
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
    PneumaticLoadCell(uint8_t channel, uint8_t mux_addr, uint8_t sensor_idx)
      : ch_(channel), mux_(mux_addr), sensor_idx_(sensor_idx) {
      lp_.setup(MPRLS_SAMPLING_RATE_HZ, LOWPASS_CUTOFF_FREQ_HZ);
    }

    uint8_t getMuxAddress() const {
      return mux_;
    }

    uint8_t getChannel() const {
      return ch_;
    }

    uint8_t getSensorIndex() const {
      return sensor_idx_;
    }

    // Read pressure from the sensor
    boolean begin() {
      tcaselect(ch_, mux_);
      if (!mpr_.begin(MPRLS_ADDR)) {
        Serial.println("Failed to communicate with MPRLS sensor, check wiring? Please reboot after fixing.");
        delay(READING_TIMEOUT);
        return false;
      }
      return true;
    }

    // End communication with the sensor (if needed)
    void end() {
      tcadisable(mux_);
    }

    void requestMeasurement() {
      tcaselect(ch_, mux_);
      mpr_.requestData();
    }

    float readPressure() {
      tcaselect(ch_, mux_);

      prev_kPa_ = current_kPa_;
      last_timestamp_ms_ = current_timestamp_ms_;

      // mpr_.readPressure() is a BLOCKING call (approx 5-8ms)
      const uint32_t& raw_val = mpr_.readData(buffer_);
      current_kPa_ = lp_.filter(mpr_.convertToPressure(raw_val));
      current_timestamp_ms_ = millis();

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
      if (current_timestamp_ms_ == last_timestamp_ms_) {
        return 0.0f;
      }
      const float dt = (current_timestamp_ms_ - last_timestamp_ms_) / 1000.0f;  // convert to seconds
      return (current_kPa_ - prev_kPa_) / dt;  // simple finite difference; could be improved with more history
    }

    float getForceFromPressure() {
      return current_force_g_;
    }

    // Calibration procedure
    void resetZeroLoad() {
      requestMeasurement();
      delay(5); // Wait for the sensor to finish
      // readData needs a buffer; we can use the class member buffer_
      uint32_t raw = mpr_.readData(buffer_); 
      zero_kPa_ = mpr_.convertToPressure(raw);
    }

  private:
    uint8_t buffer_[4]; // buffer to hold raw data from sensor
    mprls0025pa00001a mpr_ = mprls0025pa00001a();
    Iir::Butterworth::LowPass<2> lp_;
    uint8_t ch_;           // Channel number
    uint8_t mux_;          // Multiplexer address
    uint8_t sensor_idx_;    // Sensor index for packet communication
    // std::map<double, double> calibration_map_; // Map of <pressure (kPa), force (grams)>

    float prev_kPa_ = 0.0;    // Previous pressure reading (kPa)
    float current_kPa_ = 0.0; // Latest pressure reading (kPa)
    float current_force_g_ = 0.0; // Latest force reading (grams)
    float zero_kPa_ = 0.0; // Pressure at zero load (kPa)
    float ratio_ = FORCE_TO_SENSOR_RATIO;    // Force-to-sensor ratio (grams/kPa)
    unsigned long current_timestamp_ms_ = 0.0; // Timestamp of the current reading for rate calculation
    unsigned long last_timestamp_ms_ = 0.0;  // Timestamp of the last reading for rate calculation
};
