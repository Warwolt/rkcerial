import sys
import serial
import signal

def signal_handler(sig, frame):
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
while True:
    ser_in = ser.readline().decode('utf-8').strip('\n')
    print(ser_in)
