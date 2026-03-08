#pragma once

#include "SerialTransfer.h"
#include "packets.h"

class SerialBridge {
  public:
    SerialBridge() : transfer_() {}

    void begin(Stream& _port) {
      transfer_.begin(_port);
    }

    void sendSensorPacket(const SensorPacket& packet) {
      uint16_t sendSize = 0;
      sendSize = transfer_.txObj(packet, sendSize);
      transfer_.sendData(sendSize, SENSOR);
    }

    bool receiveControlPacket(ControlPacket& packet) {
      if (transfer_.available()) {
        if (transfer_.currentPacketID() == CONTROL) {
          uint16_t recSize = 0;
          recSize = transfer_.rxObj(packet, recSize);
          if (recSize == sizeof(ControlPacket)) {
            return true;
          }
        }
      }
      return false;
    }

  private:
    SerialTransfer transfer_;
};