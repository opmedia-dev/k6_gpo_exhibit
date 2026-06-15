# Firmware Flashing Guide

Step-by-step instructions for flashing the K6 GPO Exhibit firmware to an ESP32 for the first time. Covers Windows, macOS, and Linux.

## What You Need

- **ESP32 DevKit** (ESP32-WROOM-32 or similar) — the board that goes on the carrier PCB
- **USB cable** — micro-USB or USB-C depending on your ESP32 board
- **Computer** — Windows 10/11, macOS 12+, or Ubuntu/Debian Linux
- **Internet connection** — to download tools and libraries (first time only)

---

## Step 1: Install Python

PlatformIO requires Python 3.6 or later. Check if you already have it:

```
python3 --version
```

### Windows

1. Download the latest Python from [python.org/downloads](https://www.python.org/downloads/)
2. Run the installer
3. **Important:** Tick the box that says **"Add Python to PATH"** before clicking Install
4. Open a new Command Prompt and verify: `python --version`

### macOS

Python 3 is included on recent macOS versions. If not:

```bash
brew install python
```

If you don't have Homebrew, install it first from [brew.sh](https://brew.sh).

### Linux (Ubuntu/Debian)

```bash
sudo apt update
sudo apt install python3 python3-pip python3-venv
```

---

## Step 2: Install PlatformIO

PlatformIO is the build system used to compile and flash the firmware. Install it via pip:

### All Platforms

```bash
pip install platformio
```

Or if `pip` isn't on your PATH:

```bash
python3 -m pip install platformio
```

Verify it installed correctly:

```bash
pio --version
```

You should see something like `PlatformIO Core, version 6.x.x`.

> **Alternative: VS Code Extension**
>
> If you prefer a graphical interface, install [Visual Studio Code](https://code.visualstudio.com/) and then install the **PlatformIO IDE** extension from the Extensions marketplace. This gives you a GUI for building and flashing. The commands below still work in the VS Code terminal.

---

## Step 3: Install USB Drivers

The ESP32 DevKit uses a USB-to-serial chip to communicate with your computer. You may need to install the driver for it.

### Identify Your Chip

Look at the largest chip near the USB connector on your ESP32 board:

| Chip | Markings | Driver |
|------|----------|--------|
| CP2102 | "SILABS CP2102" | [Silicon Labs CP210x](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers) |
| CH340 | "CH340G" or "CH340C" | [WCH CH340](http://www.wch-ic.com/downloads/CH341SER_ZIP.html) |
| CP2104 | "CP2104" | Same as CP2102 (Silicon Labs) |

### Windows

1. Download the driver for your chip from the link above
2. Run the installer
3. Restart your computer if prompted
4. Plug in the ESP32 — it should appear in Device Manager under **Ports (COM & LPT)** as `COM3`, `COM4`, etc.

### macOS

- **CP2102**: Download and install from [Silicon Labs](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers). You may need to allow the kernel extension in **System Settings → Privacy & Security**.
- **CH340**: Download and install from [WCH](http://www.wch-ic.com/downloads/CH341SER_MAC_ZIP.html). On macOS Ventura+, you may need to allow the driver in **System Settings → Privacy & Security**.

After installing, plug in the ESP32. Check it appears:

```bash
ls /dev/cu.usbserial* /dev/cu.SLAB_USBtoUART* 2>/dev/null
```

You should see something like `/dev/cu.usbserial-0001` or `/dev/cu.SLAB_USBtoUART`.

### Linux (Ubuntu/Debian)

Most Linux kernels include drivers for both CP2102 and CH340 out of the box. No manual installation needed.

Plug in the ESP32 and check:

```bash
ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null
```

You should see `/dev/ttyUSB0` or similar.

**Permission fix** — if you get "Permission denied" errors when flashing:

```bash
sudo usermod -a -G dialout $USER
```

Then **log out and back in** (or restart) for the group change to take effect.

---

## Step 4: Download the Project

Clone or download the repository:

```bash
git clone https://github.com/opmedia-dev/k6_gpo_exhibit.git
cd k6_gpo_exhibit
```

Or download as a ZIP from GitHub and extract it.

---

## Step 5: Connect the ESP32

1. Plug the ESP32 into your computer via USB
2. Wait a few seconds for the driver to initialise
3. Confirm the device is detected:

**Windows:**
Open Device Manager → Ports (COM & LPT). Note the COM port number (e.g., `COM3`).

**macOS:**
```bash
ls /dev/cu.usbserial* /dev/cu.SLAB_USBtoUART*
```

**Linux:**
```bash
ls /dev/ttyUSB* /dev/ttyACM*
```

If nothing appears, try a different USB cable — some cables are charge-only and don't carry data.

---

## Step 6: Compile the Firmware

Navigate to the firmware directory and compile:

```bash
cd firmware
pio run
```

The first time you run this, PlatformIO will automatically:
- Download the ESP32 platform tools (~200MB)
- Download the Arduino framework for ESP32
- Download the required libraries (ESP32-audioI2S, ArduinoJson)
- Compile the firmware

This may take **2-5 minutes** the first time. Subsequent builds take ~10 seconds.

You should see output ending with:

```
RAM:   [==        ]  19.5% (used 63856 bytes from 327680 bytes)
Flash: [=======   ]  66.4% (used 1304733 bytes from 1966080 bytes)
========================= [SUCCESS] Took X.XX seconds =========================
```

If you see `[SUCCESS]`, the firmware compiled correctly and you're ready to flash.

---

## Step 7: Flash the Firmware

With the ESP32 still plugged in:

```bash
pio run -t upload
```

PlatformIO will automatically detect the correct port. If it can't find the ESP32, specify the port manually:

**Windows:**
```bash
pio run -t upload --upload-port COM3
```

**macOS:**
```bash
pio run -t upload --upload-port /dev/cu.usbserial-0001
```

**Linux:**
```bash
pio run -t upload --upload-port /dev/ttyUSB0
```

You should see:

```
Uploading .pio/build/esp32dev/firmware.bin
esptool.py v4.x
...
Writing at 0x00010000... (100%)
Wrote XXXXXX bytes
Hash of data verified.
Leaving...
Hard resetting via RTS pin...
========================= [SUCCESS] Took X.XX seconds =========================
```

The ESP32 will automatically reboot with the new firmware.

---

## Step 8: Verify It's Working

Open the serial monitor to see the boot output:

```bash
pio device monitor
```

You should see output similar to:

```
[boot] K6 GPO Exhibit v1.2.0
[boot] SD card: OK
[boot] WiFi AP: K6-Exhibit (password: phonebox)
[boot] Web server started at 192.168.4.1
[boot] Ready
```

Press `Ctrl+C` to exit the serial monitor.

### Connect to the Web Interface

1. On your phone or laptop, connect to the Wi-Fi network **K6-Exhibit** (password: **phonebox**)
2. Open a browser and go to **http://192.168.4.1/**
3. You should see the K6 GPO Exhibit management interface

---

## Step 9: Prepare the SD Card

Before the exhibit is fully functional, you need an SD card with the required audio files. See [`docs/audio-files.md`](audio-files.md) for the complete list.

**Quick summary:**

1. Format a micro-SD card as **FAT32**
2. Create these directories: `/system/`, `/history/`, `/numbers/`
3. Add the required system files:
   - `/system/dialtone.mp3`
   - `/system/busy.mp3`
   - `/system/not_recognised.mp3`
   - `/system/ringing_tone.mp3`
4. Add at least one history track to `/history/` (e.g., `001.mp3`)
5. Insert the SD card into the ESP32's SD card module
6. Power cycle the ESP32

The `/logs/` and `/plugins/` directories are created automatically by the firmware.

You can also upload audio files wirelessly via the web interface after the SD card is inserted.

---

## Troubleshooting

### "No such file or directory: platformio" or "pio: command not found"

PlatformIO isn't on your PATH. Try running it via Python:

```bash
python3 -m platformio run
```

Or add the PlatformIO installation directory to your PATH:
- **Windows:** `%USERPROFILE%\.platformio\penv\Scripts`
- **macOS/Linux:** `~/.platformio/penv/bin`

### "Error: Failed to connect to ESP32: Timed out waiting for packet header"

- Press and hold the **BOOT** button on the ESP32 board while the upload starts, then release it when you see "Connecting..."
- Try a different USB cable (must be a data cable, not charge-only)
- Try a different USB port (preferably directly on the computer, not a hub)

### "Permission denied" on Linux

```bash
sudo usermod -a -G dialout $USER
```

Log out and back in, then retry.

### "Permission denied" on macOS (driver not loaded)

Go to **System Settings → Privacy & Security** and click **Allow** next to the blocked driver message. Then unplug and replug the ESP32.

### Compilation errors

Make sure you're in the `firmware/` directory (not the project root):

```bash
cd k6_gpo_exhibit/firmware
pio run
```

### ESP32 keeps rebooting (crash loop)

Connect the serial monitor (`pio device monitor`) and check the error output. Common causes:
- SD card not inserted or not formatted as FAT32
- SD card module not wired correctly

The firmware enters safe mode after 3 consecutive crashes within 10 seconds. In safe mode, only the Wi-Fi access point and web interface are active — you can use this to diagnose the issue or flash new firmware via OTA.

### "SD card: FAIL" in serial output

- Check the SD card is inserted and making good contact
- Verify the SD card is formatted as FAT32 (not exFAT or NTFS)
- Check the SPI wiring: CS=GPIO 5, MOSI=GPIO 23, MISO=GPIO 19, SCK=GPIO 18

---

## Updating Firmware Later

After the first flash, you have two options for updating:

### Option 1: USB (same as above)

```bash
cd firmware
pio run -t upload
```

### Option 2: Wireless (OTA)

1. Connect to the **K6-Exhibit** Wi-Fi network
2. Open **http://192.168.4.1/**
3. Scroll to **Update Firmware**
4. Click **Choose File** and select the `.bin` file from `firmware/.pio/build/esp32dev/firmware.bin`
5. Click **Install Update**
6. Wait for the upload to complete — the device will restart automatically

The firmware uses A/B partitioning, so if an OTA update fails, it automatically rolls back to the previous working version after 3 failed boots.
