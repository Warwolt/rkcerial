import sys
import serial
import signal
from datetime import datetime

def signal_handler(sig, frame):
    print("[Monitor] serial port monitor closed")
    sys.exit(0)

signal.signal(signal.SIGINT, signal_handler)

if len(sys.argv) < 2:
    print("Usage: monitor.py PORT")
    print("")
    print("PORT should be the serial port name on your OS, e.g. COM2 on Windows or ttyS0 on Linux")
    exit(1)

baud = 9600
port = sys.argv[1]
ser = serial.Serial(port, baud)
# We use a quirke where opening serial communication with the Arduino Uno will
# reset it, and therefore send the time stamp message after we receive the first
# message from the Arduino.
has_sent_time = False

print("[Monitor] serial port monitor opened (close with ctrl+C)")

while True:
    # Wait until receive a line
    ser_in = ser.readline().decode('utf-8').strip('\n')
    print(ser_in)

    # If not yet sent current time, send it
    if not has_sent_time:
        has_sent_time = True
        current_time = datetime.now()
        time_str = "TIME {} {} {} {}".format(
            current_time.hour,
            current_time.minute,
            current_time.minute,
            current_time.microsecond // 1000,
        )
        ser.write(time_str.encode())
