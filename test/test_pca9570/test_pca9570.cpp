#include <unity.h>
#include "config.h"
#include "tca9548a.h"
#include "pca9570.hpp"

#define PCA9570_MUX_ADDR 0x70
constexpr uint8_t LED_ON = LOW;   // for active-low wiring
constexpr uint8_t LED_OFF = HIGH;

// GPIO object
pca9570 led;

static int8_t g_found_channel = -1;

static int8_t find_pca9570_channel_on_mux(uint8_t mux_addr) {
  for (uint8_t ch = TCA9548A_MIN_CHANNEL; ch <= TCA9548A_MAX_CHANNEL; ch++) {
    int sel = tcaselect(ch, mux_addr);
    if (sel != 0) {
      continue;
    }

    Wire.beginTransmission(PCA9570_SLAVE_ADDRESS);
    if (Wire.endTransmission() == 0) {
      return (int8_t)ch;
    }
  }
  return -1;
}

// Unit tests
void test_tca9548a_present_at_0x70(void) {
  int res = tcadisable(PCA9570_MUX_ADDR);
  TEST_ASSERT_EQUAL(0, res);
}

void test_find_pca9570_on_tca0x70(void) {
  g_found_channel = find_pca9570_channel_on_mux(PCA9570_MUX_ADDR);
  TEST_ASSERT_TRUE_MESSAGE(g_found_channel >= 0, "PCA9570 (0x24) not found on any channel of TCA9548A at 0x70");
}

void test_pca9570_read_write_latch(void) {
  TEST_ASSERT_TRUE_MESSAGE(g_found_channel >= 0, "No PCA9570 channel cached from discovery test");

  int sel = tcaselect((uint8_t)g_found_channel, PCA9570_MUX_ADDR);
  TEST_ASSERT_EQUAL(0, sel);

  TEST_ASSERT_TRUE(led.begin(Wire));

  led.writeOutput(0x00);
  delay(500);
  TEST_ASSERT_EQUAL_UINT8(0x00, led.readOutput());

  led.writeOutput(PCA9570_PIN_MASK);
  delay(500);
  TEST_ASSERT_EQUAL_UINT8(PCA9570_PIN_MASK, led.readOutput());

  led.digitalWrite(1, LED_ON);
  delay(500);
  TEST_ASSERT_EQUAL(LED_ON, led.digitalRead(1));

  led.digitalWrite(1, LED_OFF);
  delay(500);
  TEST_ASSERT_EQUAL(LED_OFF, led.digitalRead(1));

  led.digitalWrite(2, LED_ON);
  delay(500);
  TEST_ASSERT_EQUAL(LED_ON, led.digitalRead(2));

  led.digitalWrite(2, LED_OFF);
  delay(500);
  TEST_ASSERT_EQUAL(LED_OFF, led.digitalRead(2));

  led.digitalWrite(3, LED_ON);
  delay(500);
  TEST_ASSERT_EQUAL(LED_ON, led.digitalRead(3));

  led.digitalWrite(3, LED_OFF);
  delay(500);
  TEST_ASSERT_EQUAL(LED_OFF, led.digitalRead(3));

  tcadisable(PCA9570_MUX_ADDR);
}

static void run_all_tests(void) {
  for (const auto& mux : TCAADDR_ADDRESSES) {
    tcadisable(mux);  // Ensure mux is disabled before scanning
  }
  UNITY_BEGIN();
  RUN_TEST(test_tca9548a_present_at_0x70);
  RUN_TEST(test_find_pca9570_on_tca0x70);
  RUN_TEST(test_pca9570_read_write_latch);
  UNITY_END();
}

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);
  run_all_tests();
}

void loop() {
  // nothing
}
