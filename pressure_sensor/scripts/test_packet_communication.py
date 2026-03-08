# test_packet_communication.py
import serial
from serial.tools import list_ports
import argparse
from pathlib import Path
import time
import threading
from enum import IntFlag
from abc import ABC, abstractmethod
import cutie
from pySerialTransfer.pySerialTransfer import SerialTransfer, Status, STRUCT_FORMAT_LENGTHS

BAUD = 115200

class PacketID(IntFlag):
    PACKET_CONTROL = 0x00
    PACKET_SENSOR  = 0x01

class ControlFlags(IntFlag):
    CTRL_ACK  = 1 << 0  # 1 = ACK, 0 = NACK
    CTRL_BUSY = 1 << 1  # sensor busy
    CTRL_ERR  = 1 << 2  # error present

class RequestType(IntFlag):
    REQUEST_RESET_ZERO_LOAD = 0x00
    REQUEST_CALIBRATION     = 0x01

class Packet(ABC):
    def __init__(self):
        self.sensor_idx = 0

    def serialize(self, link):
        keys = self.__dict__.keys()
        sendSize = 0
        for k in keys:
            v = self.__dict__[k]
            sendSize = link.tx_obj(v, start_pos=sendSize)
        return sendSize

    @abstractmethod
    def deserialize(self, link):
        pass

    def to_str(self):
        return str(self.__dict__)

class ControlPacket(Packet):
    def __init__(self):
        super().__init__()
        self.sensor_idx = 0
        self.request_idx = int(RequestType.REQUEST_NONE)
        self.flags = int(0)
        self.error_code = int(0)
    
    def deserialize(self, link):
        recSize = 0
        self.sensor_idx = link.rx_obj(obj_type='b', start_pos=recSize)
        recSize += STRUCT_FORMAT_LENGTHS['b']
        self.request_idx = link.rx_obj(obj_type='b', start_pos=recSize)
        recSize += STRUCT_FORMAT_LENGTHS['b']
        self.flags = link.rx_obj(obj_type='b', start_pos=recSize)
        recSize += STRUCT_FORMAT_LENGTHS['b']
        self.error_code = link.rx_obj(obj_type='b', start_pos=recSize)

class SensorPacket(Packet):
    def __init__(self):
        super().__init__()
        self.sensor_idx = int(0)
        self.sensor_pressure_kPa = float(0.0)
        self.sensor_pressure_rate_kPa_s = float(0.0)
        self.sensor_force_g = float(0.0)

    def deserialize(self, link):
        recSize = 0
        self.sensor_idx = link.rx_obj(obj_type='b', start_pos=recSize)
        recSize += STRUCT_FORMAT_LENGTHS['b']
        self.sensor_pressure_kPa = link.rx_obj(obj_type='f', start_pos=recSize)
        recSize += STRUCT_FORMAT_LENGTHS['f']
        self.sensor_pressure_rate_kPa_s = link.rx_obj(obj_type='f', start_pos=recSize)
        recSize += STRUCT_FORMAT_LENGTHS['f']
        self.sensor_force_g = link.rx_obj(obj_type='f', start_pos=recSize)

def terminal_menu(choices, title="Select an option"):
    choices_with_caption = [title] + [str(c) for c in choices]
    selected_idx = cutie.select(choices_with_caption, caption_indices=[0])
    return choices[selected_idx - 1]

def autodetect_port():
    ports = list(list_ports.comports())
    if not ports:
        raise RuntimeError("No serial ports found.")
    if len(ports) == 1:
        return ports[0].device

    print("Available ports:")
    for i, p in enumerate(ports):
        print(f"  [{i}] {p.device}  {p.description}")
    idx = int(input("Select port index: "))
    return ports[idx].device


def sendPacket(link, pkt: Packet):
    '''
    Helper function to send a control packet to the Arduino. The control packet
    is defined in the packets.h file on the Arduino and is used to send commands
    or requests from the Python script to the Arduino.
    
    Parameters:
        link (SerialTransfer): The SerialTransfer link object used for communication.
        pkt (Packet): The packet to send.
    '''

    # Serialize and send the control packet
    send_size = pkt.serialize(link)
    link.send(send_size)

def receivePacket(link):
    '''
    Helper function to receive a packet from the Arduino. The packet type can be
    specified to determine how to parse the incoming data.
    
    Parameters:
        link (SerialTransfer): The SerialTransfer link object used for communication.

    Returns:
        An instance of the received packet type with the parsed data, or None if no packet is available.
    '''
    if link.id_byte == PacketID.PACKET_SENSOR:
        pkt = SensorPacket()
        pkt.deserialize(link)
        return pkt
    elif link.id_byte == PacketID.PACKET_CONTROL:
        pkt = ControlPacket()
        pkt.deserialize(link)
        return pkt


def controlRequestThread(link):
    '''
    Thread function to periodically send control packets to the Arduino. This can be used
    to request sensor data or send commands at regular intervals.
    '''
    options = [RequestType.REQUEST_CALIBRATION, RequestType.REQUEST_RESET_ZERO_LOAD]

    while True:
        request = terminal_menu(options, title="Select a control request to send:")
        pkt = ControlPacket()
        pkt.sensor_idx = 0  # example sensor index
        pkt.request_idx = request
        sendPacket(link, pkt)
        time.sleep(5)

            

if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        prog="test_packet_communication.py",
        description="Test script for packet communication between Python and Arduino using pySerialTransfer. \
                     This script will send control packets to the Arduino and receive sensor packets in response, \
                     printing the received data to the console.",
    )
    parser.add_argument(
        "--sampling-interval",
        type=float,
        default=0.001,
        help="seconds between each check for incoming packets (default: 0.001s)",
    )

    args = parser.parse_args()
    sampling_interval = float(args.sampling_interval)

    port = autodetect_port()
    try:
        link = SerialTransfer(port, baud=BAUD)

        link.open()
        time.sleep(2) # allow some time for the Arduino to completely reset

        transmitter_thread = threading.Thread(target=controlRequestThread, args=(link,), daemon=True)
        transmitter_thread.start()
        print("Transmitter thread started.")

        print("Receiver thread started.")
        lastTime = time.time()
        while True:
            if (time.time() - lastTime) > sampling_interval:
                lastTime = time.time()

                if link.available():    # reads a packet
                    pkt = receivePacket(link)
                    if pkt and isinstance(pkt, SensorPacket):
                        print(f"Received Sensor Packet: {pkt.to_str()}")
                    elif isinstance(pkt, ControlPacket):
                        print(f"Received Control ACK for Sensor {pkt.sensor_idx}")

                elif link.status.value < 0:
                    if link.status == Status.CRC_ERROR:
                        print('ERROR: CRC_ERROR')
                    elif link.status == Status.PAYLOAD_ERROR:
                        print('ERROR: PAYLOAD_ERROR')
                    elif link.status == Status.STOP_BYTE_ERROR:
                        print('ERROR: STOP_BYTE_ERROR')
                    else:
                        print(f'ERROR: {link.status.name}')

    except KeyboardInterrupt:
        try:
            link.close()
        except:
            pass
    
    except:
        import traceback
        traceback.print_exc()
        
        try:
            link.close()
        except:
            pass
