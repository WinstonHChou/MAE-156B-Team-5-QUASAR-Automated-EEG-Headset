#pragma once

#include <Wire.h>

#define PCA9570_SLAVE_ADDRESS 0x24  ///< Default I2C address for PCA9570 (can be changed by hardware address pins)

#define PCA9570_GENERAL_CALL_ADDR 0x00   ///< General call address (software reset)
#define PCA9570_DEVICE_ID_WRITE   0xF8   ///< Device ID write address
#define PCA9570_DEVICE_ID_READ    0xF9   ///< Device ID read address
#define PCA9570_SOFT_RESET_CMD    0x06   ///< Software reset command (see datasheet §7.2.1)
#define PCA9570_PINS_COUNT        4      ///< Number of GPIO pins on the chip
#define PCA9570_PIN_MASK          0x0F   ///< Lower 4 GPIO bits used by the device

class pca9570 {
  public:
    pca9570() : wire_(nullptr), output_shadow_(PCA9570_PIN_MASK) {}

    bool begin(TwoWire& wire = Wire) {
      wire_ = &wire;
      wire_->beginTransmission(PCA9570_SLAVE_ADDRESS);
      if (wire_->endTransmission() == 0) {
        // Default all pins HIGH to keep active-low LEDs off and reset de-asserted.
        writeOutput(output_shadow_);
        return true;
      }
      return false; // Initialization failed
    }

    void writeOutput(uint8_t value) {
      if (wire_ == nullptr) return;
      wire_->beginTransmission(PCA9570_SLAVE_ADDRESS);
      wire_->write(value & PCA9570_PIN_MASK);
      wire_->endTransmission();
    }

    uint8_t readOutput() {
      if (wire_ == nullptr) return PCA9570_PIN_MASK;
      uint8_t value;
      if (wire_->requestFrom((uint8_t)PCA9570_SLAVE_ADDRESS, (uint8_t)1) == 1) {
        value = wire_->read();
      } else {
        value = PCA9570_PIN_MASK; // Return all high on valid GPIO pins if read fails
      }
      return value & PCA9570_PIN_MASK;
    }

    void digitalWrite(uint8_t pin, uint8_t value) {
      if (pin >= PCA9570_PINS_COUNT) return; // Invalid pin number
      if (value == HIGH) {
        output_shadow_ |= (1 << pin);  // Set bit to drive high
      } else {
        output_shadow_ &= ~(1 << pin); // Clear bit to drive low
      }
      writeOutput(output_shadow_ & PCA9570_PIN_MASK);
    }

    int digitalRead(uint8_t pin) {
      if (pin >= PCA9570_PINS_COUNT) return LOW; // Invalid pin number
      uint8_t state = readOutput();
      return (state & (1 << pin)) ? HIGH : LOW;
    }

  private:
    TwoWire* wire_;
    uint8_t output_shadow_;
};
