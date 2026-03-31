# RPLidar Persistent Port Configuration (Ubuntu 24.04)

## Objective

When working with USB devices such as the RPLidar, the assigned device path (e.g. `/dev/ttyUSB0`) may change after rebooting or reconnecting the device. This introduces instability in robotic systems (e.g. ROS 2 nodes), where a fixed serial port is expected.

The goal of this guide is to create a **persistent device alias** using `udev` rules.

---

## Step 1 — Identify Device Attributes

### Command

```bash
udevadm info -q property -n /dev/ttyUSB0 | egrep 'ID_VENDOR_ID|ID_MODEL_ID|ID_SERIAL'
```

### Step-by-step explanation

* `udevadm`

  * Tool to query and control the Linux device manager (`udev`)

* `info`

  * Subcommand used to extract information about a device

* `-q property`

  * Query mode: returns **key-value properties** (clean and script-friendly)

* `-n /dev/ttyUSB0`

  * Specifies the device node to inspect

* `|` (pipe)

  * Sends output to another command

* `egrep 'ID_VENDOR_ID|ID_MODEL_ID|ID_SERIAL'`

  * Filters only relevant identifiers using regex

### Example Output

```
ID_MODEL_ID=ea60
ID_SERIAL=Silicon_Labs_CP2102_USB_to_UART_Bridge_Controller_0001
ID_SERIAL_SHORT=0001
ID_VENDOR_ID=10c4
```

### Interpretation

* `ID_VENDOR_ID` → Manufacturer (Silicon Labs)
* `ID_MODEL_ID` → Device type (USB-UART bridge)
* `ID_SERIAL_SHORT` → Unique hardware identifier (preferred for precision)

---

## Step 2 — Create a udev Rule

### Command

```bash
sudo nano /etc/udev/rules.d/99-rplidar.rules
```

### Step-by-step explanation

* `sudo`

  * Executes the command with administrative privileges (required to modify `/etc`)

* `nano`

  * Terminal-based text editor

* `/etc/udev/rules.d/`

  * Directory where custom udev rules are stored

* `99-rplidar.rules`

  * File name:

    * `99-` → high priority (executed after default rules)
    * `rplidar` → descriptive name

---

### Rule to add

```bash
SUBSYSTEM=="tty", ATTRS{idVendor}=="10c4", ATTRS{idProduct}=="ea60", ATTRS{serial}=="0001", SYMLINK+="rplidar"
```

---

## Concept: udev Rules

A `udev` rule defines how the system should react when a device is detected.

### Structure

```
<MATCH CONDITIONS>, <ACTIONS>
```

### Detailed breakdown of the rule

* `SUBSYSTEM=="tty"`

  * Matches serial devices (UART over USB)

* `ATTRS{idVendor}=="10c4"`

  * Matches USB vendor ID

* `ATTRS{idProduct}=="ea60"`

  * Matches product ID

* `ATTRS{serial}=="0001"`

  * Matches a **specific physical device**

* `SYMLINK+="rplidar"`

  * Action: creates a symbolic link `/dev/rplidar`
  * `+=` means "append" (does not override existing links)

---

## Concept: Symbolic Links (symlinks)

A symbolic link is a pointer to another file or device.

Example:

```
/dev/rplidar -> /dev/ttyUSB0
```

### Why this matters

* `/dev/ttyUSB0` is **dynamic** (can change)
* `/dev/rplidar` is **stable** (controlled by your rule)

This abstraction is critical in robotics systems where device paths must be deterministic.

---

## Step 3 — Reload udev Rules

### Commands

```bash
sudo udevadm control --reload-rules
sudo udevadm trigger
```

### Step-by-step explanation

#### 1. Reload rules

* `udevadm control`

  * Interface to control the udev daemon

* `--reload-rules`

  * Reloads all rule files without restarting the system

#### 2. Apply rules

* `udevadm trigger`

  * Re-emits device events so rules are applied immediately

---

## Step 4 — Verification

### Command

```bash
ls -l /dev/rplidar
```

### Explanation

* `ls -l`

  * Lists files in long format

* Output example:

```
/dev/rplidar -> /dev/ttyUSB0
```

* `->` indicates a symbolic link

---

## Usage in ROS 2

Instead of using a dynamic port:

```
/dev/ttyUSB0
```

Use the persistent alias:

```
/dev/rplidar
```

### Example

```bash
ros2 launch rplidar_ros rplidar.launch.py serial_port:=/dev/rplidar
```

---

## Summary

* USB device names (`ttyUSB0`) are not reliable
* `udev` rules provide deterministic device mapping
* Symlinks abstract unstable kernel naming
* This is a required practice in production robotics systems

---

## Notes

* Always prefer `ID_SERIAL_SHORT` or `ATTRS{serial}` when multiple devices exist
* Rules are evaluated in lexical order (`99-` ensures priority)
* If changes do not apply, reconnect the device
