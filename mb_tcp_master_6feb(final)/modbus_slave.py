import logging
import asyncio
from pymodbus.server import StartAsyncTcpServer
from pymodbus.datastore import ModbusSequentialDataBlock, ModbusSlaveContext, ModbusServerContext
from pymodbus.device import ModbusDeviceIdentification

# ==========================================
# USER CONFIGURATION (Edit this section easily)
# ==========================================
# The IP address your PC will use (Must match what you type in ESP32: IP0=...)
SERVER_IP = "0.0.0.0"  # 0.0.0.0 listens on ALL your network cards (WiFi, Ethernet, etc)
SERVER_PORT = 502
SLAVE_ID = 1           # Must match MB_DEVICE_ADDR1 = 1 in your C code

# This map is just for printing logs so you know what the ESP32 is changing.
# Based on your C code's "reg_start" values.
REGISTER_MAP = {
    0: "AC Unit (On/Off)",
    1: "AC Mode (Auto/Heat/Cool...)",
    2: "Fan Speed",
    3: "Vane Position",
    4: "Temp Setpoint",
    5: "Temp Reference",
    6: "Window Contact",
    7: "Adapter Enable",
    8: "Remote Control",
    9: "Operation Time",
    10: "Alarm Status",
    11: "Error Code",
    12: "Ambient Temp",
    13: "Real Setpoint",
    14: "Max Setpoint",
    15: "Min Setpoint",
    22: "Ambient Temp (Duplicate/Alt)",
    31: "Status Feedback",
    34: "Vane Pulse",
    66: "Return Path Temp",
    98: "Gateway/Slave Info"
}

# ==========================================
# LOGGING SETUP
# ==========================================
logging.basicConfig()
log = logging.getLogger()
log.setLevel(logging.INFO)

# ==========================================
# CUSTOM DATA BLOCK (To print changes)
# ==========================================
class MonitoringDataBlock(ModbusSequentialDataBlock):
    """
    A generic data block that prints to console whenever 
    the ESP32 writes a value to us.
    """
    def setValues(self, address, values):
        # First, actually save the value so the Modbus logic works
        super().setValues(address, values)
        
        # Now print what happened
        for i, val in enumerate(values):
            reg_addr = address + i
            # Look up the name in our map, or just call it "Unknown Register"
            reg_name = REGISTER_MAP.get(reg_addr, f"Register {reg_addr}")
            
            print(f"[WRITE RECEIVED] ESP32 wrote to {reg_name}: {val}")

# ==========================================
# MAIN SERVER LOGIC
# ==========================================
async def run_server():
    # 1. Create the storage
    # We allocate 100 registers (address 0 to 99) initializing them to 0
    # We use our custom class 'MonitoringDataBlock' so we can see print statements
    store = ModbusSlaveContext(
        di=None, co=None, ir=None, 
        hr=MonitoringDataBlock(0, [0] * 120)  # hr = Holding Registers
    )
    
    # 2. Define the context (Slave ID 1)
    context = ModbusServerContext(slaves={SLAVE_ID: store}, single=False)

    # 3. Define Identity (Optional, makes it look professional)
    identity = ModbusDeviceIdentification()
    identity.VendorName = "My Generic Python Simulator"
    identity.ProductCode = "ESP32-TESTER"
    identity.VendorUrl = "http://localhost"
    identity.ProductName = "AC Unit Simulator"
    identity.ModelName = "Simulated AC"
    identity.MajorMinorRevision = "1.0"

    print(f"--- STARTING MODBUS SERVER ---")
    print(f"Listening on: {SERVER_IP}:{SERVER_PORT}")
    print(f"Simulating Slave ID: {SLAVE_ID}")
    print(f"Use ESP32 Command: IP0=192.168.4.101 (or your PC IP)")
    print(f"------------------------------")

    # 4. Start the server
    await StartAsyncTcpServer(
        context=context, 
        identity=identity, 
        address=(SERVER_IP, SERVER_PORT)
    )

if __name__ == "__main__":
    try:
        # Run the async server
        asyncio.run(run_server())
    except KeyboardInterrupt:
        print("\nStopping server...")
    except PermissionError:
        print("\n[ERROR] Permission denied. On Linux/Mac, ports below 1024 require sudo.")
        print("Try changing SERVER_PORT to 5020 in the script and in your ESP32 code.")