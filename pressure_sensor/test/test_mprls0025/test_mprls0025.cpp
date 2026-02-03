#include <unity.h>
#include "mprls_pressure_sensor.h"

#define TEST_DURATION_MS 100000

// Sensor object
Adafruit_MPRLS mpr = Adafruit_MPRLS(RESET_PIN, EOC_PIN);

// Unit tests
void test_mprls0025_pressure(void) {
  Serial.begin(115200);
  long start_time = millis();
  TEST_MESSAGE("Starting MPRLS 0025 pressure sensor test...");
  for (uint8_t i = TCA9548A_MIN_CHANNEL; i <= TCA9548A_MAX_CHANNEL; i++) {
    int res = tcaselect(i);
    TEST_ASSERT_EQUAL(0, res); // expect success (0) when TCA present/responding

    Wire.beginTransmission(MPRLS_DEFAULT_ADDR);
    if (!Wire.endTransmission()) {
      TEST_MESSAGE("TCA Port #");
      char buf[4];
      snprintf(buf, sizeof(buf), "%u", i);
      TEST_MESSAGE(buf);
      TEST_MESSAGE("Found MPRLS at address 0x");
      char addr_buf[4];
      snprintf(addr_buf, sizeof(addr_buf), "%02X", MPRLS_DEFAULT_ADDR);
      TEST_MESSAGE(addr_buf);

      bool begun = mpr.begin(MPRLS_DEFAULT_ADDR);
      TEST_ASSERT_TRUE(begun);

      while (millis() - start_time < TEST_DURATION_MS) {
        float pressure = mpr.readPressure();
        TEST_MESSAGE("Pressure reading (hPa): ");
        char pres_buf[16];
        snprintf(pres_buf, sizeof(pres_buf), "%.2f", pressure);
        TEST_MESSAGE(pres_buf);

        delay(MPRLS_SAMPLING_RATE_MS);
      }
    }
  }
  TEST_MESSAGE("Done");
}

// void test_mprls0025_calibrated_force(void) {
//   long start_time = millis();
//   TEST_MESSAGE("Starting MPRLS 0025 calibrated force sensor test...");
//   for (uint8_t i = TCA9548A_MIN_CHANNEL; i <= TCA9548A_MAX_CHANNEL; i++) {
//     int res = tcaselect(i);
//     TEST_ASSERT_EQUAL(0, res); // expect success (0) when TCA present/responding

//     Wire.beginTransmission(MPRLS_DEFAULT_ADDR);
//     if (!Wire.endTransmission()) {
//       TEST_MESSAGE("TCA Port #");
//       char buf[4];
//       snprintf(buf, sizeof(buf), "%u", i);
//       TEST_MESSAGE(buf);
//       TEST_MESSAGE("Found MPRLS at address 0x");
//       char addr_buf[4];
//       snprintf(addr_buf, sizeof(addr_buf), "%02X", MPRLS_DEFAULT_ADDR);
//       TEST_MESSAGE(addr_buf);

//       bool begun = mpr.begin(MPRLS_DEFAULT_ADDR);
//       TEST_ASSERT_TRUE(begun);

//       while (millis() - start_time < TEST_DURATION_MS) {
//         float pressure = mpr.readPressure();
//         TEST_MESSAGE("Pressure reading (hPa): ");
//         char pres_buf[16];
//         snprintf(pres_buf, sizeof(pres_buf), "%.2f", pressure);
//         TEST_MESSAGE(pres_buf);

//         delay(MPRLS_SAMPLING_RATE_MS);
//       }
//     }
//   }
//   TEST_MESSAGE("Done");
// }

static void run_all_tests(void) {
  UNITY_BEGIN();
  RUN_TEST(test_mprls0025_pressure);
  // RUN_TEST(test_mprls0025_calibrated_force);
  UNITY_END();
}

void setup() {
  Wire.begin();
  run_all_tests();
}

void loop() {
  // nothing
}
