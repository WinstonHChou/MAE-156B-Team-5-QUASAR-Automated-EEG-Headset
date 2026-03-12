#pragma once

#include "config.h"
#include "tca9548a.h"
#include "mprls0025pa00001a.hpp"
#include "pca9570.hpp"

#include <map>
#include <iterator>
#include <limits>
#include <Iir.h>

#define MPRLS_ADDR MPRLS_DEFAULT_ADDR
#define PSI_to_KPA (6.8947572932f)   ///< Constant: PSI to KPA conversion factor

#define GPIO_HARDWARE_RESET_PIN       0         // GPIO pin for hardware reset control (if needed)

#define LED_ON                        LOW       // for active-low wiring
#define LED_OFF                       HIGH
#define LED_STATUS_PIN                1         // Status LED on sensor breakout board (P1)
#define LED_TARING_STATUS_PIN         2         // Taring status LED on sensor breakout board (P2)
#define LED_HARDWARE_RESET_STATUS_PIN 3         // Hardware reset indicator LED on breakout board (P3)
#define LED_BLINK_INTERVAL_MS         500       // Interval for blinking the LED in busy status


// Pneumatic Load Cell
class PneumaticLoadCell {
  public:
    enum SensorStatus {
      OK = 0,
      BUSY = 1,
      FAILURE = 2
    };

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

    SensorStatus getStatus() const {
      return status_;
    }

    bool isHardwareResetInProgress() const {
      return hardware_reset_triggered_;
    }

    // Read pressure from the sensor
    boolean begin() {
      tcaselect(ch_, mux_);
      if (!sensor_.begin(MPRLS_ADDR)) {
        Serial.println("Failed to communicate with MPRLS sensor, check wiring? Please reboot after fixing.");
        delay(READING_TIMEOUT);
        return false;
      }

      status_led_ = new pca9570();
      if (!status_led_->begin()) {
        Serial.println("Failed to communicate with PCA9570 status LED, check wiring? Please reboot after fixing.");
        delay(READING_TIMEOUT);
        delete status_led_;
        status_led_ = nullptr; // Set to nullptr to indicate LED is not available, but we can still operate the sensor
      }
      return true;
    }

    void requestMeasurement() {
      if (hardware_reset_triggered_) return; // If hardware reset is active, skip requesting measurement
      tcaselect(ch_, mux_);
      sensor_.requestData();
    }

    void update() {
      tcaselect(ch_, mux_);

      prev_kPa_ = current_kPa_;
      last_timestamp_ms_ = current_timestamp_ms_;

      // Handle status LED:
      if (status_led_) {
        uint8_t desired_led_state = LED_OFF;
        switch (status_) {
          case OK:
            desired_led_state = LED_ON;
            break;
          case BUSY:
            // Blink the LED to indicate busy status
            desired_led_state = (millis() / LED_BLINK_INTERVAL_MS) % 2 == 0 ? LED_ON : LED_OFF;
            break;
          case FAILURE:
            desired_led_state = LED_OFF;
            break;
        }
        if (desired_led_state != last_led_state_) {
          status_led_->digitalWrite(LED_STATUS_PIN, desired_led_state);
          last_led_state_ = desired_led_state;
        }
      }

      // If sensor is in terminal failure state, avoid repeated I2C reads that can stall the loop.
      if (status_ == FAILURE && !hardware_reset_triggered_) {
        current_timestamp_ms_ = millis();
        return;
      }

      // Always short-circuit while reset is in progress, even if the LED expander is unavailable.
      if (hardware_reset_triggered_) {
        if (millis() - last_reset_time_ms_ > HARDWARE_RESET_TIMEOUT_MS) {
          if (status_led_) {
            status_led_->digitalWrite(GPIO_HARDWARE_RESET_PIN, HIGH); // De-assert reset
            status_led_->digitalWrite(LED_HARDWARE_RESET_STATUS_PIN, LED_OFF); // Indicate hardware reset is ended
          }
          hardware_reset_triggered_ = false;
          status_ = OK;
        }
        current_timestamp_ms_ = millis();
        return; // Skip the rest of update while hardware reset is active
      }

      // Read raw pressure data from the sensor
      const uint32_t raw_val = sensor_.readData(buffer_);
      if (raw_val == 0xFFFFFFFF) {
        status_ = FAILURE;
        current_timestamp_ms_ = millis();
        return;
      }

      const float pressure_kPa = sensor_.convertToPressure(raw_val);
      if (isnan(pressure_kPa)) {
        status_ = FAILURE;
        current_timestamp_ms_ = millis();
        return;
      }

      current_kPa_ = lp_.filter(pressure_kPa);
      current_timestamp_ms_ = millis();

      if (taring_triggered_ && millis() - last_taring_time_ms_ > TARING_TIMEOUT_MS) {
        if (status_led_) {
          status_led_->digitalWrite(LED_TARING_STATUS_PIN, LED_OFF); // Indicate taring is completed
        }
        taring_triggered_ = false;
        zero_kPa_ = current_kPa_;
        prev_kPa_ = current_kPa_;      // Reset previous reading to avoid large spikes
        accumulated_drift_kPa_ = 0.0f; // Reset accumulated drift when zero load is reset
        is_even_sample_ = true; // Reset sample count for Simpson's rule
      }

      // Estimator pipeline: exponential decay model for drift compensation
      if (abs(getPressureRate()) > MIN_ACCEPTABLE_PRESSURE_RATE_THRESHOLD_KPA_S) {
        // option 1: Right Riemann sum approximation of the integral of the pressure difference over time
        // accumulated_drift_kPa_ +=
        //     (1 / DRIFT_TIME_CONSTANT_S) * (current_kPa_ - ambient_kPa_) * (current_timestamp_ms_ - last_timestamp_ms_) / 1000.0f;

        // option 2: Simpson's 1/3 rule approximation of the integral, which can be more accurate with fewer samples, but requires storing one more previous reading
        if (is_even_sample_) {
          accumulated_drift_kPa_ +=
              (1 / DRIFT_TIME_CONSTANT_S) * (current_kPa_ - ambient_kPa_) * (MPRLS_SAMPLING_INTERVAL_MS / 1000.0f) / 3.0f * 4; // Odd samples get quadruple weight in Simpson's rule
        } else {
          accumulated_drift_kPa_ +=
              (1 / DRIFT_TIME_CONSTANT_S) * (current_kPa_ - ambient_kPa_) * (MPRLS_SAMPLING_INTERVAL_MS / 1000.0f) / 3.0f * 2; // Even samples get double weight in Simpson's rule
        }
      }
      is_even_sample_ = !is_even_sample_; // Toggle sample parity

      float estimated_pressure_kPa_ = current_kPa_ + accumulated_drift_kPa_;
      current_force_g_ = (estimated_pressure_kPa_ - zero_kPa_) * ratio_;  // in grams
    }

    float getPressure() {
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
      if (status_led_) {
        status_led_->digitalWrite(LED_TARING_STATUS_PIN, LED_ON); // Indicate taring in progress
      }
      taring_triggered_ = true;
      last_taring_time_ms_ = millis();
    }

    void setToCalibrationMode() {
      status_ = BUSY;
    }

    void setToNormalMode() {
      status_ = OK;
    }

    void setRatio(float ratio) {
      ratio_ = ratio;
    }

    bool resetHardware() {
      if (status_led_ && status_led_->digitalRead(GPIO_HARDWARE_RESET_PIN) == HIGH) {
        status_led_->digitalWrite(GPIO_HARDWARE_RESET_PIN, LOW); // Assert reset
        status_led_->digitalWrite(LED_HARDWARE_RESET_STATUS_PIN, LED_ON); // Indicate hardware reset in progress
        hardware_reset_triggered_ = true;
        status_ = FAILURE; // Set status to FAILURE during reset
        last_reset_time_ms_ = millis();
      }
      return hardware_reset_triggered_;
    }

    static void updateAmbientPressure(float ambient_kPa) {
      ambient_kPa_ = ambient_kPa;
    }

  private:
    uint8_t buffer_[4]; // buffer to hold raw data from sensor
    mprls0025pa00001a sensor_ = mprls0025pa00001a();
    Iir::Butterworth::LowPass<2> lp_;
    uint8_t ch_;           // Channel number
    uint8_t mux_;          // Multiplexer address
    uint8_t sensor_idx_;    // Sensor index for packet communication

    pca9570* status_led_ = nullptr; // Pointer to status LED object
    SensorStatus status_ = OK; // Current status of the sensor
    uint8_t last_led_state_ = LED_OFF; // Track last LED state to avoid redundant writes

    boolean hardware_reset_triggered_ = false;
    unsigned long last_reset_time_ms_ = 0; // Timestamp of the last reset for timeout handling

    boolean taring_triggered_ = false;
    unsigned long last_taring_time_ms_ = 0; // Timestamp of the last taring for timeout handling

    float prev_kPa_ = 0.0;    // Previous pressure reading (kPa)
    float current_kPa_ = 0.0; // Latest pressure reading (kPa)
    float current_force_g_ = 0.0; // Latest force reading (grams)
    float zero_kPa_ = 0.0; // Pressure at zero load (kPa)
    inline static float ambient_kPa_ = DEFAULT_AMBIENT_PRESSURE_KPA; // Ambient pressure for reference (kPa)
    float accumulated_drift_kPa_ = 0.0; // Accumulated drift in pressure (kPa) for compensation
    boolean is_even_sample_ = true; // Flag to track even/odd samples for Simpson's rule
    float ratio_ = FORCE_TO_SENSOR_RATIO;    // Force-to-sensor ratio (grams/kPa)
    unsigned long current_timestamp_ms_ = 0; // Timestamp of the current reading for rate calculation
    unsigned long last_timestamp_ms_ = 0;  // Timestamp of the last reading for rate calculation
};
