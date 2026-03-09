/*!
 * @file mprls0025pa00001a.cpp
*
 * Credit to Adafruit for the original MPRLS library,
 * which we modified for our use case. The original library can be found here:
 * https://github.com/adafruit/Adafruit_MPRLS
 */

#if (ARDUINO >= 100)
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

#include "mprls0025pa00001a.hpp"

/**************************************************************************/
/*!
    @brief constructor initializes default configuration value
    @param reset_pin Optional hardware reset pin, default set to -1 to skip
    @param EOC_pin Optional End-of-Convert indication pin, default set to -1 to
   skip
    @param PSI_min The minimum PSI measurement range of the sensor, default 0
    @param PSI_max The maximum PSI measurement range of the sensor, default 25
    @param OUTPUT_min The minimum transfer function curve value in %, default
   10%
    @param OUTPUT_max The maximum transfer function curve value in %, default
   90%
    @param K Conversion Factor to desired units, default is PSI to HPA
*/
/**************************************************************************/
mprls0025pa00001a::mprls0025pa00001a(int8_t reset_pin, int8_t EOC_pin,
                               uint16_t PSI_min, uint16_t PSI_max,
                               float OUTPUT_min, float OUTPUT_max, float K) {

  _reset = reset_pin;
  _eoc = EOC_pin;
  _PSI_min = PSI_min;
  _PSI_max = PSI_max;
  _OUTPUT_min = (uint32_t)((float)COUNTS_224 * (OUTPUT_min / 100.0) + 0.5);
  _OUTPUT_max = (uint32_t)((float)COUNTS_224 * (OUTPUT_max / 100.0) + 0.5);
  _K = K;
}

/**************************************************************************/
/*!
    @brief  setup and initialize communication with the hardware
    @param i2c_addr The I2C address for the sensor (default is 0x18)
    @param twoWire Optional pointer to the desired TwoWire I2C object. Defaults
   to &Wire
    @returns True on success, False if sensor not found
*/
/**************************************************************************/
boolean mprls0025pa00001a::begin(uint8_t i2c_addr, TwoWire *twoWire) {
  if (i2c_dev)
    delete i2c_dev;
  i2c_dev = new Adafruit_I2CDevice(i2c_addr, twoWire);
  if (!i2c_dev->begin())
    return false;

  if (_reset != -1) {
    pinMode(_reset, OUTPUT);
    digitalWrite(_reset, HIGH);
    digitalWrite(_reset, LOW);
    delay(10);
    digitalWrite(_reset, HIGH);
  }
  if (_eoc != -1) {
    pinMode(_eoc, INPUT);
  }

  delay(10); // startup timing

  // Serial.print("Status: ");
  // Serial.println(stat);
  return ((readStatus() & MPRLS_STATUS_MASK) == MPRLS_STATUS_POWERED);
}

/**************************************************************************/
/*!
    @brief Convert raw 24-bit ADC reading to pressure in hPa using the provided
    @param raw_psi The raw 24-bit ADC reading from the sensor, 
                  which should be preceded by a call to readData()
    @returns The measured pressure, in hPa on success, NAN on failure
*/
/**************************************************************************/
float mprls0025pa00001a::convertToPressure(uint32_t raw_psi) {
  if (raw_psi == 0xFFFFFFFF || _OUTPUT_min == _OUTPUT_max) {
    return NAN;
  }

  // All is good, calculate and convert to desired units using provided factor
  // use the 10-90 calibration curve by default or whatever provided by the user
  float psi = (raw_psi - _OUTPUT_min) * (_PSI_max - _PSI_min);
  psi /= (float)(_OUTPUT_max - _OUTPUT_min);
  psi += _PSI_min;
  // convert to desired units
  return psi * _K;
}

/**************************************************************************/
/*!
    @brief Request 24 bits of measurement data from the device
    @returns buffer containing the status byte and 24 bits of data, or NULL on failure (check status)
*/
/**************************************************************************/
void mprls0025pa00001a::requestData() {
  uint8_t cmd[3] = {0xAA, 0, 0};
  i2c_dev->write(cmd, 3);
}

/**************************************************************************/
/*!
    @brief Read 24 bits of measurement data from the device,
           Must be preceded by a call to requestData()
    @param buffer A pointer to a 4-byte buffer to store the status byte and 24 bits of data
    @returns -1 on failure (check status) or 24 bits of raw ADC reading
*/
/**************************************************************************/
uint32_t mprls0025pa00001a::readData(uint8_t* buffer) {
  // Use the gpio to tell end of conversion
  uint32_t t = millis();
  if (_eoc != -1) {
    while (!digitalRead(_eoc)) {
      if (millis() - t > MPRLS_READ_TIMEOUT)
        return 0xFFFFFFFF; // timeout
    }
  } else {
    // check the status byte
    //    uint8_t stat;
    while ((lastStatus = readStatus()) & MPRLS_STATUS_BUSY) {
      // Serial.print("Status: "); Serial.println(stat, HEX);
      if (millis() - t > MPRLS_READ_TIMEOUT)
        return 0xFFFFFFFF; // timeout
    }
  }

  // Read status byte and data
  i2c_dev->read(buffer, 4);

  // check status byte
  if (buffer[0] & MPRLS_STATUS_MATHSAT) {
    return 0xFFFFFFFF;
  }
  if (buffer[0] & MPRLS_STATUS_FAILED) {
    return 0xFFFFFFFF;
  }

  // all good, return data
  return (uint32_t(buffer[1]) << 16) | (uint32_t(buffer[2]) << 8) |
         (uint32_t(buffer[3]));
}

/**************************************************************************/
/*!
    @brief Read just the status byte, see datasheet for bit definitions
    @returns 8 bits of status data
*/
/**************************************************************************/
uint8_t mprls0025pa00001a::readStatus(void) {
  uint8_t buffer[1];
  i2c_dev->read(buffer, 1);
  return buffer[0];
}