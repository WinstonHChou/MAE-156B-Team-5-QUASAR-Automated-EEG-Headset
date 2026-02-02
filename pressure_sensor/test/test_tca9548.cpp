/*
  PlatformIO Unity unit tests for TCA9548 `tcaselect` behavior.
  This variant uses the real TwoWire `Wire` from <Wire.h> and makes
  `tcaselect()` return the `endTransmission()` result so tests can
  assert I2C call outcomes when running on hardware.

  Based on https://learn.adafruit.com/adafruit-tca9548a-1-to-8-i2c-multiplexer-breakout/arduino-wiring-and-test
*/

#include <Arduino.h>
#include <Wire.h>
#include <unity.h>
#include "Adafruit_MPRLS.h"

#define TCAADDR 0x70

// Function under test: return transmission result (0 == success)
int tcaselect(uint8_t i) {
  if (i > 7) return -1;
  Wire.beginTransmission(TCAADDR);
  Wire.write(1 << i);
  return Wire.endTransmission();
}

// Unit tests
void test_tcaselect_valid_ports(void) {
  for (uint8_t i = 0; i < 8; i++) {
    int res = tcaselect(i);
    TEST_ASSERT_EQUAL(0, res); // expect success (0) when TCA present/responding

    TEST_MESSAGE("TCA Port #");
    char buf[4];
    snprintf(buf, sizeof(buf), "%u", i);
    TEST_MESSAGE(buf);
    for (uint8_t addr = 0; addr <= 127; addr++) {
      if (addr == TCAADDR) continue;

      Wire.beginTransmission(addr);
      if (!Wire.endTransmission()) {
        TEST_MESSAGE("Found I2C 0x");
        char addr_buf[5];
        snprintf(addr_buf, sizeof(addr_buf), "%02X", addr);
        TEST_MESSAGE(addr_buf);
      }
    }
  }
  TEST_MESSAGE("Done");
}

void test_tcaselect_invalid_port(void) {
  int res = tcaselect(8); // out of range
  TEST_ASSERT_EQUAL(-1, res);
}

static void run_all_tests(void) {
  UNITY_BEGIN();
  RUN_TEST(test_tcaselect_valid_ports);
  RUN_TEST(test_tcaselect_invalid_port);
  UNITY_END();
}

void setup() {
  Wire.begin();
  run_all_tests();
}

void loop() {
  // nothing
}
