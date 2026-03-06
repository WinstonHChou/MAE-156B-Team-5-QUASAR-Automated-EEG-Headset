/**
 * @file packets.h
 * @brief Packet definitions for serial communication between microcontroller and host computer
 * 
 * Defines the packet structures and control flags used for serial protocol communication
 * between the host computer and microcontroller for sensor data acquisition and control.
 */
#pragma once
#include <stdint.h>


/**
 * @enum ControlFlags
 * @brief Bitmask flags for control packet status and responses
 * 
 * These flags are used in the ControlPacket.flags field to indicate packet status,
 * acknowledgment, and error conditions.
 * 
 * @var CTRL_ACK
 *      Acknowledgment flag (1 = ACK received, 0 = NACK)
 * @var CTRL_BUSY
 *      Sensor busy flag (1 = sensor is busy processing, 0 = ready)
 * @var CTRL_ERR
 *      Error flag (1 = error present, 0 = no error)
 * 
 * @note Usage example on receiving side:
 *       bool isAck = (pkt.flags & CTRL_ACK) != 0;
 *       bool hasErr = (pkt.flags & CTRL_ERR) != 0;
 */
enum ControlFlags : uint8_t {
    CTRL_ACK  = 1 << 0,  // 1 = ACK, 0 = NACK
    CTRL_BUSY = 1 << 1,  // sensor busy
    CTRL_ERR  = 1 << 2   // error present
};

enum RequestType : uint8_t {
    REQUEST_RESET_ZERO_LOAD = 0,
    REQUEST_CALIBRATION = 1,
    // Add more request types as needed
};


/**
 * @struct ControlPacket
 * @brief Command packet sent from host to microcontroller
 * 
 * Used to request data from sensors or send control commands to the microcontroller.
 * Supports addressing up to 2 TCA9548A multiplexers (16 sensors).
 * 
 * @var sensor_idx
 *      Index of the target sensor (0-15 for up to 2 muxes)
 * @var request_idx
 *      Type of request (0 = reset zero load, 1 = calibration, etc.)
 * @var flags
 *      Bitmask for control flags (see ControlFlags enum)
 * @var error_code
 *      Optional error code (0 = no error)
 */
typedef struct __attribute__((packed)) {
    uint8_t sensor_idx;
    uint8_t request_idx;   // request type
    uint8_t flags;         // bitmask: ACK/BUSY/ERR
    uint8_t error_code;    // optional: 0 = none
} ControlPacket;


/**
 * @struct SensorPacket
 * @brief Sensor data packet sent from microcontroller to host
 * 
 * Contains real-time sensor measurements including pressure and force data.
 * 
 * @var sensor_idx
 *      Index of the source sensor
 * @var sensor_pressure_kPa
 *      Absolute pressure reading in kilopascals
 * @var sensor_pressure_rate_kPa_s
 *      Rate of pressure change in kilopascals per second
 * @var sensor_force_g
 *      Force measurement in grams-force (gf)
 */
typedef struct __attribute__((packed)) {
    uint8_t sensor_idx;
    float sensor_pressure_kPa;
    float sensor_pressure_rate_kPa_s;
    float sensor_force_g;
} SensorPacket;
