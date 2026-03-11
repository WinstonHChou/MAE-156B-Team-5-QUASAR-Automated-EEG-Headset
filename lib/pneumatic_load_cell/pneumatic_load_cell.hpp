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
#define LED_HARDWARE_RESET_STATUS_PIN 2         // Hardware reset indicator LED on breakout board (P2)


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
      tcaselect(ch_, mux_);
      if (hardware_reset_triggered_) return; // If hardware reset is active, skip requesting measurement
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
            desired_led_state = (millis() / 500) % 2 == 0 ? LED_ON : LED_OFF;
            break;
          case FAILURE:
            desired_led_state = LED_OFF;
            break;
        }
        if (desired_led_state != last_led_state_) {
          status_led_->digitalWrite(LED_STATUS_PIN, desired_led_state);
          last_led_state_ = desired_led_state;
        }

        if (hardware_reset_triggered_) {
          if (millis() - last_reset_time_ms_ > HARDWARE_RESET_TIMEOUT_MS) {
            // If hardware reset is active, override to indicate reset status
            status_led_->digitalWrite(GPIO_HARDWARE_RESET_PIN, HIGH); // Assert reset
            status_led_->digitalWrite(LED_HARDWARE_RESET_STATUS_PIN, LED_OFF); // Indicate hardware reset is ended
            hardware_reset_triggered_ = false; // Reset state back to inactive after asserting
            status_ = OK; // Assume sensor will be OK after reset
          }
          return; // Skip the rest of the update while in hardware reset
        }
      }

      // Read raw pressure data from the sensor
      const uint32_t& raw_val = sensor_.readData(buffer_);
      current_kPa_ = lp_.filter(sensor_.convertToPressure(raw_val));
      current_timestamp_ms_ = millis();

      // Estimator pipeline: exponential decay model for drift compensation
      accumulated_drift_kPa_ +=
          (1 / DRIFT_TIME_CONSTANT_S) * (current_kPa_ - ambient_kPa_) * (current_timestamp_ms_ - last_timestamp_ms_) / 1000.0f;
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
      requestMeasurement();
      delay(5); // Wait for the sensor to finish
      // readData needs a buffer; we can use the class member buffer_
      uint32_t raw = sensor_.readData(buffer_); 
      zero_kPa_ = sensor_.convertToPressure(raw);
      accumulated_drift_kPa_ = 0.0f; // Reset accumulated drift when zero load is reset
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

    void resetHardware() {
      if (status_led_ && status_led_->digitalRead(GPIO_HARDWARE_RESET_PIN) == HIGH) {
        status_led_->digitalWrite(GPIO_HARDWARE_RESET_PIN, LOW); // Assert reset
        status_led_->digitalWrite(LED_HARDWARE_RESET_STATUS_PIN, LED_ON); // Indicate hardware reset in progress
        hardware_reset_triggered_ = true;
        status_ = FAILURE; // Set status to FAILURE during reset
        last_reset_time_ms_ = millis();
      }
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

    float prev_kPa_ = 0.0;    // Previous pressure reading (kPa)
    float current_kPa_ = 0.0; // Latest pressure reading (kPa)
    float current_force_g_ = 0.0; // Latest force reading (grams)
    float zero_kPa_ = 0.0; // Pressure at zero load (kPa)
    static float ambient_kPa_ = DEFAULT_AMBIENT_PRESSURE_KPA; // Ambient pressure for reference (kPa)
    float accumulated_drift_kPa_ = 0.0; // Accumulated drift in pressure (kPa) for compensation
    float ratio_ = FORCE_TO_SENSOR_RATIO;    // Force-to-sensor ratio (grams/kPa)
    unsigned long current_timestamp_ms_ = 0; // Timestamp of the current reading for rate calculation
    unsigned long last_timestamp_ms_ = 0;  // Timestamp of the last reading for rate calculation
};
