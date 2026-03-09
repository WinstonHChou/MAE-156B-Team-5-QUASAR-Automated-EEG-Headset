#pragma once

#include "SerialTransfer.h"
#include "packets.h"

class SerialBridge {
  public:
    SerialBridge() : transfer_() {}

    void begin(Stream& _port) {
      transfer_.begin(_port);
    }

    void send(const SensorPacket& pkt) {
      uint16_t sendSize = 0;
      sendSize = transfer_.txObj(pkt, sendSize);
      transfer_.sendData(sendSize, SENSOR);
    }

    void send(const ControlPacket& pkt) {
      uint16_t sendSize = 0;
      sendSize = transfer_.txObj(pkt, sendSize);
      transfer_.sendData(sendSize, CONTROL);
    }

    bool receive(ControlPacket& pkt) {
      if (transfer_.available()) {
        if (transfer_.currentPacketID() == CONTROL) {
          uint16_t recSize = 0;
          recSize = transfer_.rxObj(pkt, recSize);
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