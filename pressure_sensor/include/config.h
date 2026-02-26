// You dont *need* a reset and EOC pin for most uses, so we set to -1 and don't connect
#define RESET_PIN  -1  // set to any GPIO pin # to hard-reset on begin()
#define EOC_PIN    -1  // set to any GPIO pin to read end-of-conversion by pin
#define MPRLS_ADDR MPRLS_DEFAULT_ADDR

#define MPRLS_SAMPLING_INTERVAL_MS  10      // delay between pressure reads
#define MPRLS_SAMPLING_RATE_HZ  1000.0f / MPRLS_SAMPLING_INTERVAL_MS
#define READING_TIMEOUT         10      // ms to wait for end-of-conversion before giving up

#define LOWPASS_ORDER           2       // Butterworth low-pass filter order
#define LOWPASS_CUTOFF_FREQ_HZ  3.0f    // Hz, lower = smoother but more lag

#define TCAADDR_ADDRESSES {0x72, 0x73, 0x74, 0x75}

// Calibration Coefficients
#define FORCE_TO_SENSOR_RATIO 56.436f  // grams/kPa, calibrated on 2026/02/02
#define MIN_ACCEPTABLE_PRESSURE_RATE_THRESHOLD_KPA_S 0.1f  // Min pressure rate to accept (kPa/s), to filter out drifts
