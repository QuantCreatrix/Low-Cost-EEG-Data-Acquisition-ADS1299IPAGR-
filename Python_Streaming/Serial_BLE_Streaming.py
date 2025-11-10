import serial
import struct
import time
from datetime import datetime

class BluetoothEEGReceiver:
    def __init__(self, port=None):
        self.ser = None
        self.running = False
        self.last_sample_time = time.time()
        self.manual_port = port  # User-specified port
        self.reconnect_attempts = 0
        self.max_reconnect_attempts = 5

    def connect(self):
        """Establish Bluetooth connection with retries"""
        while not self.running and self.reconnect_attempts < self.max_reconnect_attempts:
            try:
                if self.manual_port:
                    port = self.manual_port
                else:
                    print("No port specified. Please provide the COM port.")
                    return False
                
                print(f"Attempting to connect to {port}...")
                self.ser = serial.Serial(
                    port=port,
                    baudrate=115200,
                    timeout=1,
                    write_timeout=1
                )
                
                # Wait for connection to stabilize
                time.sleep(2)
                
                if self.ser.is_open:
                    print("Successfully connected!")
                    self.running = True
                    self.reconnect_attempts = 0  # Reset on successful connection
                    return True
                    
            except Exception as e:
                self.reconnect_attempts += 1
                print(f"Connection failed (attempt {self.reconnect_attempts}/{self.max_reconnect_attempts}): {str(e)}")
                if self.ser:
                    self.ser.close()
                if self.reconnect_attempts < self.max_reconnect_attempts:
                    print(f"Retrying in 3 seconds...")
                    time.sleep(3)
        
        if self.reconnect_attempts >= self.max_reconnect_attempts:
            print("Max reconnection attempts reached. Please check your Bluetooth connection.")
        return False

    def read_samples(self):
        """Generator that yields EEG samples"""
        while self.running:
            try:
                # Check for disconnection
                if not self.ser or not self.ser.is_open:
                    print("Connection lost! Attempting to reconnect...")
                    self.running = False
                    self.ser = None
                    self.connect()
                    continue
                
                # Wait for enough data to be available
                if self.ser.in_waiting >= NUM_CHANNELS * 4:  # 4 bytes per float
                    raw_data = self.ser.read(NUM_CHANNELS * 4)
                    
                    # Verify we got the right amount of data
                    if len(raw_data) == NUM_CHANNELS * 4:
                        # Unpack binary data to floats
                        sample = struct.unpack(f'{NUM_CHANNELS}f', raw_data)
                        timestamp = time.time()
                        
                        # Calculate sampling rate (for monitoring)
                        current_time = time.time()
                        time_diff = current_time - self.last_sample_time
                        self.last_sample_time = current_time
                        
                        yield timestamp, sample
                    else:
                        print(f"Incomplete sample received: {len(raw_data)} bytes")
                        # Clear buffer to resync
                        self.ser.reset_input_buffer()
                
                # Small sleep to prevent CPU overuse
                time.sleep(0.001)
                        
            except serial.SerialException as e:
                print(f"Serial error: {str(e)}")
                self.handle_disconnection()
            except struct.error as e:
                print(f"Data unpacking error: {str(e)}")
                # Clear buffer to resync
                self.ser.reset_input_buffer()
            except Exception as e:
                print(f"Unexpected error: {str(e)}")
                self.handle_disconnection()
    
    def handle_disconnection(self):
        """Handle disconnection and attempt reconnect"""
        print("Disconnection detected")
        self.running = False
        if self.ser:
            self.ser.close()
            self.ser = None
        self.connect()
    
    def start(self):
        """Start the receiver"""
        if self.connect():
            try:
                for timestamp, sample in self.read_samples():
                    # Print the raw data (replace with LSL push_sample() when ready)
                    print(f"{datetime.fromtimestamp(timestamp).strftime('%H:%M:%S.%f')}: {sample}")
                    
            except KeyboardInterrupt:
                print("\nStopping receiver...")
            finally:
                if self.ser and self.ser.is_open:
                    self.ser.close()
                self.running = False

# Configuration
NUM_CHANNELS = 4  # Must match ESP32 code

if __name__ == "__main__":
    # Manually specify your COM port here (e.g., "COM6")
    PORT = input("Enter the COM port (e.g., COM6): ").strip()
    
    receiver = BluetoothEEGReceiver(port=PORT)
    receiver.start()
