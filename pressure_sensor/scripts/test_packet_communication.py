# test_packet_communication.py
import serial
from serial.tools import list_ports
from packets import PacketID, ControlFlags, RequestType, ControlPacket, SensorPacket, SerialBridge
import argparse
from pathlib import Path
import time
import threading
import cutie


BAUD = 115200

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
    parser.add_argument(
        "--debug-sensor",
        action="store_true",
        help="Enable sensor debug mode to print additional information.",
    )

    args = parser.parse_args()
    sampling_interval = float(args.sampling_interval)

    port = autodetect_port()
    try:
        bridge = SerialBridge(port, BAUD, debug=args.debug_sensor)

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
