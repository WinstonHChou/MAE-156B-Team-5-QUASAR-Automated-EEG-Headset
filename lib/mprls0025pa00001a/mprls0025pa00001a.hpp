/**
 * Credit to Adafruit for the original MPRLS library,
 * which we modified for our use case. The original library can be found here:
 * https://github.com/adafruit/Adafruit_MPRLS
 */

#pragma once

#if (ARDUINO >= 100)
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

#include <Adafruit_I2CDevice.h>

#define MPRLS_DEFAULT_ADDR (0x18)   ///< Most common I2C address
#define MPRLS_READ_TIMEOUT (20)     ///< millis
#define MPRLS_STATUS_POWERED (0x40) ///< Status SPI powered bit
#define MPRLS_STATUS_BUSY (0x20)    ///< Status busy bit
#define MPRLS_STATUS_FAILED (0x04)  ///< Status bit for integrity fail
#define MPRLS_STATUS_MATHSAT (0x01) ///< Status bit for math saturation
#define COUNTS_224 (16777216L)      ///< Constant: 2^24
#define PSI_to_KPA (6.8947572932f)  ///< Constant: PSI to KPA conversion factor
#define MPRLS_STATUS_MASK                                                      \
  (0b01100101) ///< Sensor status mask: only these bits are set

/**************************************************************************/
/*!
    @brief  Class that stores state and functions for interacting with MPRLS
   sensor IC
*/
/**************************************************************************/
class mprls0025pa00001a {
public:
  mprls0025pa00001a(int8_t reset_pin = -1, int8_t EOC_pin = -1,
                     uint16_t PSI_min = 0, uint16_t PSI_max = 25,
                     float OUTPUT_min = 10, float OUTPUT_max = 90,
                     float K = PSI_to_KPA);

  bool begin(uint8_t i2c_addr = MPRLS_DEFAULT_ADDR, TwoWire *twoWire = &Wire);

  void requestData();
  uint32_t readData(uint8_t* buffer);

  uint8_t readStatus(void);
  float convertToPressure(uint32_t raw_psi);

  uint8_t lastStatus; /*!< status byte after last operation */

private:
  Adafruit_I2CDevice *i2c_dev = NULL; ///< Pointer to I2C bus interface

  int8_t _reset, _eoc;
  uint16_t _PSI_min, _PSI_max;
  uint32_t _OUTPUT_min, _OUTPUT_max;
  float _K;
};
