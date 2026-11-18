import serial
import time
import sys

def main():
    port = "COM6"
    baud = 115200
    timeout_sec = 40
    
    print(f"Opening {port} at {baud}...")
    try:
        ser = serial.Serial(port, baud, timeout=1)
    except Exception as e:
        print(f"Failed to open {port}: {e}")
        return

    start_time = time.time()
    try:
        while time.time() - start_time < timeout_sec:
            line = ser.readline()
            if line:
                try:
                    text = line.decode('utf-8', errors='replace').rstrip()
                    print(text)
                    sys.stdout.flush()
                except Exception:
                    pass
    except KeyboardInterrupt:
        pass
    finally:
        ser.close()
        print("Serial port closed.")

if __name__ == '__main__':
    main()
