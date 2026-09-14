import time
import serial, subprocess, os
HOME = os.path.expanduser("~")
def toggle_keyboard(): subprocess.Popen(["bash", HOME + "/sanad/keyboard.sh"])
time.sleep(15)
print("Sanad listener started...", flush=True)
ser = serial.Serial("/dev/ttyACM0", 115200, timeout=1)
while True:
    line = ser.readline().decode("utf-8", errors="ignore").strip()
    if not line: continue
    if "KEYBOARD_ON" in line or "KEYBOARD_OFF" in line:
        print(">> TOGGLE KEYBOARD", flush=True)
        toggle_keyboard()
    elif "VOICE" in line:
        print(">> VOICE TRIGGERED", flush=True)
        subprocess.Popen(["bash", HOME + "/sanad/voice.sh"])
