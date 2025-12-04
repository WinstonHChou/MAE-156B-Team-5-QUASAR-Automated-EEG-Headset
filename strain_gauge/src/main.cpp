#include <hx711.h>

// The current setup is using SPI
#define DOUT_PIN   A1  // Hx711.DOUT - pin #A1
#define SCK_PIN    A0  // Hx711.SCK - pin #A0

Hx711 scale(DOUT_PIN, SCK_PIN);

// timestamps
uint64_t timeStamp;

void setup() {
  // Begin
  Serial.begin(115200);
  while (!Serial) delay(10); // wait until Serial becomes avaliable

  // if (!imu.begin(BNO08X_ADDR, Wire, BNO08X_INT, BNO08X_RST)) {
  //   Serial.println("IMU not found!");
  //   while(1);
  // }
  // Serial.println("BNO08x found!");
}


void loop() {

  Serial.print(scale.getGram(), 1);
  Serial.println(" g");

  delay(20);
  // delay(1);
}
