#pragma once

typedef struct __attribute__((packed)) {
    uint8_t sensor_idx;
    float sensor_pressure_kPa;
    float sensor_force_g;
} SensorPacket;

typedef struct __attribute__((packed)) {
    uint8_t sensor_idx;
    uint8_t request_idx;    // e.g., 0 for zero-load calibration, 1 for tare, etc.
} ControlPacket;

