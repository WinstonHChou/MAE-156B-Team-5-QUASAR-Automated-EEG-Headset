#pragma once
#include <cstdint>

// #define DEBUG_SERIAL

// 1. Hardware Pins
#define SDA_PIN     22  // not ESP32 default SDA pin (21), it's reversed on the breakout board, so we have to specify it here
#define SCL_PIN     21  // not ESP32 default SCL pin (22), it's reversed on the breakout board, so we have to specify it here

// 2. Communication Parameters
#define I2C_MIN_ADDRESS 0x00
#define I2C_MAX_ADDRESS 0x7F
#define I2C_STANDARD_MODE_CLOCK_FREQ 100000L
#define I2C_FAST_MODE_CLOCK_FREQ 400000L

#define I2C_CLOCK_FREQ I2C_FAST_MODE_CLOCK_FREQ
#define BRIDGE_BAUDRATE 921600  // need headroom for 13+ sensor packets per cycle (was 460800; 13th sensor still dropped)
#define WAIT_FOR_CONVERSION_TIME_MS 5   // Longest sensor conversion time (typically 5ms)

// 3. The Addressing Logic
constexpr uint8_t TCAADDR_ADDRESSES[] = {0x70, 0x71, 0x72, 0x73};
#define NUM_OF_SENSOR_SLOTS (sizeof(TCAADDR_ADDRESSES) / sizeof(TCAADDR_ADDRESSES[0]) * 8)

// 4. Timing & Filtering
#define MPRLS_SAMPLING_INTERVAL_MS   20
#define MPRLS_SAMPLING_RATE_HZ       (1000.0f / MPRLS_SAMPLING_INTERVAL_MS)
#define READING_TIMEOUT              10
#define HARDWARE_RESET_TIMEOUT_MS    1000 // Timeout for hardware reset in milliseconds
#define TARING_TIMEOUT_MS            200  // Timeout for taring in milliseconds

// 5. Physics & Calibration
#define LOWPASS_ORDER                2
#define LOWPASS_CUTOFF_FREQ_HZ       3.0f
#define FORCE_TO_SENSOR_RATIO        20.0f
#define DRIFT_TIME_CONSTANT_S        8000.0f
#define MIN_ACCEPTABLE_PRESSURE_RATE_THRESHOLD_KPA_S 0.1f
#define AMBIENT_PRESSURE_SENSOR_IDX  31 // Index of the sensor used for ambient pressure reference
#define DEFAULT_AMBIENT_PRESSURE_KPA (101.3f) ///< Default ambient pressure in kPa at sea level for drift compensation
