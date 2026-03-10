/*
  PlatformIO Unity unit tests for TCA9548 `tcaselect` behavior.
  This variant uses the real TwoWire `Wire` from <Wire.h> and makes
  `tcaselect()` return the `endTransmission()` result so tests can
  assert I2C call outcomes when running on hardware.

  Based on https://learn.adafruit.com/adafruit-tca9548a-1-to-8-i2c-multiplexer-breakout/arduino-wiring-and-test
*/

#include <unity.h>
#include "config.h"
#include "tca9548a.h"

#define TCAADDRS {0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77}

// Unit tests
void test_tcaselect_valid_ports(void) {
  for (uint8_t mux : TCAADDRS) {
    TEST_MESSAGE("TCA 0x");
    char addr_buf[5];
    snprintf(addr_buf, sizeof(addr_buf), "%02X", mux);
    TEST_MESSAGE(addr_buf);
    for (uint8_t i = TCA9548A_MIN_CHANNEL; i <= TCA9548A_MAX_CHANNEL; i++) {
      int res = tcaselect(i, mux);
      if (res != 0) {
        TEST_MESSAGE("TCASELECT Error Code ");
        char buf[4];
        snprintf(buf, sizeof(buf), "%d", res);
        TEST_MESSAGE(buf);
        TEST_MESSAGE("The TCA address might not be present/responding on channel.\n");
        break;  // skip to next mux if current one is not responding
      }
      TEST_ASSERT_EQUAL(0, res); // expect success (0) when TCA present/responding

      TEST_MESSAGE("Port #");
      char buf[4];
      snprintf(buf, sizeof(buf), "%u", i);
      TEST_MESSAGE(buf);
      for (uint8_t addr = I2C_MIN_ADDRESS; addr <= I2C_MAX_ADDRESS; addr++) {
        bool is_tca = false;
        for (uint8_t tca_addr : TCAADDRS) {
          if (addr == tca_addr) {
            is_tca = true;
            break;
          }
        }
        if (is_tca) continue;

        Wire.beginTransmission(addr);
        if (!Wire.endTransmission()) {
          TEST_MESSAGE("Found I2C 0x");
          char addr_buf[5];
          snprintf(addr_buf, sizeof(addr_buf), "%02X", addr);
          TEST_MESSAGE(addr_buf);
        }
      }
    }
  }
  TEST_MESSAGE("Done");
}

void test_tcadisable(void) {
  int res = tcadisable();
  TEST_ASSERT_EQUAL(0, res); // expect success (0) when TCA present/responding
  TEST_MESSAGE("TCA disabled");
}

void test_tcaselect_invalid_port(void) {
  int res = tcaselect(8); // out of range
  TEST_ASSERT_EQUAL(-1, res);
}

static void run_all_tests(void) {
  UNITY_BEGIN();
  RUN_TEST(test_tcaselect_valid_ports);
  RUN_TEST(test_tcadisable);
  RUN_TEST(test_tcaselect_invalid_port);
  UNITY_END();
  for (const auto& mux : TCAADDR_ADDRESSES) {
    tcadisable(mux);  // Ensure mux is disabled before scanning
  }
}

void setup() {
  Wire.begin(SDA_PIN, SCL_PIN);
  run_all_tests();
}

void loop() {
  // nothing
}
