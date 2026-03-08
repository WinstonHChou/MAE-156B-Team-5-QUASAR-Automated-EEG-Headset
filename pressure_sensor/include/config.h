#pragma once
#include <cstdint>
#include "tca9548a.h"

// 1. Hardware Pins
#define RESET_PIN   -1 
#define EOC_PIN     -1 
#define SDA_PIN     22  // not ESP32 default SDA pin (21), it's reversed on the breakout board, so we have to specify it here
#define SCL_PIN     21  // not ESP32 default SCL pin (22), it's reversed on the breakout board, so we have to specify it here

// 2. Communication Parameters
#define I2C_CLOCK_FREQ I2C_FAST_MODE_CLOCK_FREQ
#define BRIDGE_BAUDRATE 115200

// 3. The Addressing Logic
#define TCAADDR_ADDRESSES {0x70, 0x71, 0x72, 0x73}
constexpr uint8_t TCA_LIST[] = TCAADDR_ADDRESSES;

#define NUM_OF_SENSOR_SLOTS (sizeof(TCA_LIST) / sizeof(TCA_LIST[0]) * 8)

// 4. Timing & Filtering
#define MPRLS_SAMPLING_INTERVAL_MS   10
#define MPRLS_SAMPLING_RATE_HZ       (1000.0f / MPRLS_SAMPLING_INTERVAL_MS)
#define READING_TIMEOUT              10

// 5. Physics & Calibration
#define LOWPASS_ORDER                2
#define LOWPASS_CUTOFF_FREQ_HZ       3.0f
#define FORCE_TO_SENSOR_RATIO        56.436f
#define MIN_ACCEPTABLE_PRESSURE_RATE_THRESHOLD_KPA_S 0.1f
