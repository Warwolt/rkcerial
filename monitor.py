import sys
import serial
import signal
from datetime import datetime


def timestamp_now():
    '''
    Returns the current time as milliseconds since 00:00
    '''
    midnight = datetime.now().replace(
        hour=0, minute=0, second=0, microsecond=0)
    delta = datetime.now() - midnight
    return delta.seconds * 1000 + delta.microseconds // 1000


def signal_handler(sig, frame):
    print("[ Monitor ] Serial port closed.")
    sys.exit(0)


if __name__ == "__main__":
    signal.signal(signal.SIGINT, signal_handler)

    if len(sys.argv) < 2:
        print("Usage: monitor.py PORT", end="")
        print("")
        print("PORT should be the serial port name on your OS, e.g. COM2 on Windows or ttyS0 on Linux")
        exit(1)

    baud = 9600
    port = sys.argv[1]
    ser = serial.Serial(port, baud, timeout=0.5)
    # We use a quirk where opening serial communication with the Arduino Uno will
    # reset it, and therefore send the time stamp message after we receive the first
    # message from the Arduino.
    has_sent_time = False

    print("[ Monitor ] Serial port opened (close with Ctrl+C).")

    while True:
        # Wait until receive a line
        ser_in = ser.readline().decode('utf-8').strip('\n')
        if ser_in:
            print(ser_in)

        # If not yet sent current time, send it
        if ser_in and not has_sent_time:
            has_sent_time = True
            time_str = "TIMENOW {}".format(timestamp_now())
            print("[ Monitor ] Sending wall clock time.")
            ser.write(time_str.encode())
