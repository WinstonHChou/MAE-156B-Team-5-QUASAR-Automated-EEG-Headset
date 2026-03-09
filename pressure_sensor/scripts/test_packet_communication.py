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
    @abstractmethod
    def serialize(self, link):
        pass

    @abstractmethod
    def deserialize(self, link):
        pass

    def to_str(self):
        return str(self.__dict__)

class ControlPacket(Packet):
    def __init__(self):
        super().__init__()
        self.sensor_idx = 0
        self.request_idx = RequestType.REQUEST_CALIBRATION
        self.flags = 0
        self.error_code = 0

    def serialize(self, link):
        sendSize = 0
        sendSize = link.tx_obj(self.sensor_idx, start_pos=sendSize, val_type_override='b')
        sendSize = link.tx_obj(self.request_idx, start_pos=sendSize, val_type_override='b')
        sendSize = link.tx_obj(self.flags, start_pos=sendSize, val_type_override='b')
        sendSize = link.tx_obj(self.error_code, start_pos=sendSize, val_type_override='b')
        return sendSize
    
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

    def serialize(self, link):
        sendSize = 0
        sendSize = link.tx_obj(self.sensor_idx, start_pos=sendSize, val_type_override='b')
        sendSize = link.tx_obj(self.sensor_pressure_kPa, start_pos=sendSize, val_type_override='f')
        sendSize = link.tx_obj(self.sensor_pressure_rate_kPa_s, start_pos=sendSize, val_type_override='f')
        sendSize = link.tx_obj(self.sensor_force_g, start_pos=sendSize, val_type_override='f')
        return sendSize

    def deserialize(self, link):
        recSize = 0
        self.sensor_idx = link.rx_obj(obj_type='b', start_pos=recSize)
        recSize += STRUCT_FORMAT_LENGTHS['b']
        self.sensor_pressure_kPa = link.rx_obj(obj_type='f', start_pos=recSize)
        recSize += STRUCT_FORMAT_LENGTHS['f']
        self.sensor_pressure_rate_kPa_s = link.rx_obj(obj_type='f', start_pos=recSize)
        recSize += STRUCT_FORMAT_LENGTHS['f']
        self.sensor_force_g = link.rx_obj(obj_type='f', start_pos=recSize)

class SerialBridge:
    def __init__(self, port, baud=BAUD):
        self.link = SerialTransfer(port, baud=BAUD)

        self.link.open()
        time.sleep(2) # allow some time for the Arduino to completely reset

    def getTransfer(self):
        return self.link

    def send(self, pkt: Packet):
        '''
        Helper function to send a control packet to the Arduino. The control packet
        is defined in the packets.h file on the Arduino and is used to send commands
        or requests from the Python script to the Arduino.
        
        Parameters:
            pkt (Packet): The packet to send.
        '''

        if type(pkt) == ControlPacket:
            packet_id = PacketID.PACKET_CONTROL
        elif type(pkt) == SensorPacket:
            packet_id = PacketID.PACKET_SENSOR
        else:
            packet_id = 0   # Default to 0
        # Serialize and send packet
        send_size = pkt.serialize(self.link)
        self.link.send(send_size, packet_id)

    def receive(self, pkt=None):
        '''
        Helper function to receive a packet from the Arduino. The packet type can be
        specified to determine how to parse the incoming data.

        Returns:
            An instance of the received packet type with the parsed data, or None if no packet is available.
        '''
        if self.link.available():    # reads a packet

            if self.link.status.value < 0:
                if self.link.status == Status.CRC_ERROR:
                    print('ERROR: CRC_ERROR')
                elif self.link.status == Status.PAYLOAD_ERROR:
                    print('ERROR: PAYLOAD_ERROR')
                elif self.link.status == Status.STOP_BYTE_ERROR:
                    print('ERROR: STOP_BYTE_ERROR')
                else:
                    print(f'ERROR: {self.link.status.name}')

            if self.link.id_byte == PacketID.PACKET_SENSOR:
                pkt = SensorPacket()
                pkt.deserialize(self.link)
                # print(f"Received Sensor Packet: {pkt.to_str()}")
                return pkt
            elif self.link.id_byte == PacketID.PACKET_CONTROL:
                pkt = ControlPacket()
                pkt.deserialize(self.link)
                if pkt.flags & ControlFlags.CTRL_ACK:
                    print(f"Received Control ACK for Sensor {pkt.sensor_idx} with request {RequestType(pkt.request_idx).name}")
                return pkt



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

def controlRequestThread(bridge: SerialBridge):
    '''
    Thread function to periodically send control packets to the Arduino. This can be used
    to request sensor data or send commands at regular intervals.
    '''
    options = [RequestType.REQUEST_RESET_ZERO_LOAD.name, RequestType.REQUEST_CALIBRATION.name]
    min_val = 0
    max_val = 31

    while True:
        request_mask = terminal_menu(options, title="Select a control request to send:")
        while True:
            user_input = input(f"Enter a sensor index (enter a number between {min_val} and {max_val}): ")
            try:
                # Convert input to an integer
                number = int(user_input)
                
                # Check if the number is within the valid range
                if min_val <= number <= max_val:
                    break # Exit the loop if valid
                else:
                    print("Invalid integer. The number must be in the specified range.")
            except ValueError:
                # Handle the error if the input cannot be converted to an integer
                print("Invalid input. Please enter a whole number.")

        pkt = ControlPacket()
        pkt.sensor_idx = number
        pkt.request_idx = RequestType[request_mask].value
        bridge.send(pkt)

        print(f"Sending Control Packet - Sensor Index: {pkt.sensor_idx}, Request Type: {RequestType(pkt.request_idx).name}")
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
        bridge = SerialBridge(port)
        
        transmitter_thread = threading.Thread(target=controlRequestThread, args=(bridge,), daemon=True)
        transmitter_thread.start()
        print("Transmitter thread started.")

        print("Receiver thread started.")
        lastTime = time.time()
        while True:
            if (time.time() - lastTime) > sampling_interval:
                lastTime = time.time()

                pkt = bridge.receive()

    except KeyboardInterrupt:
        try:
            bridge.getTransfer().close()
        except:
            pass
    
    except:
        import traceback
        traceback.print_exc()
        
        try:
            bridge.getTransfer().close()
        except:
            pass
