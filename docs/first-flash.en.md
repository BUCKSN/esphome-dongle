# First flash

Russian: [first-flash.md](first-flash.md). Windows and macOS are not verified yet; Linux is tested by the author.

You flash over wires once, later updates install from the dongle page. The stock firmware is saved and can be restored.
The appliance and its warranty remain your responsibility.

## You need

- The dongle, **removed from the appliance**.
- A 3.3 V USB-UART adapter with a 5 V output, such as CP2102, CH340 or PL2303.
- A USB bracket from an old PC: a USB-A socket and a motherboard cable with separate contacts. No soldering needed.
- A multimeter and female-to-female wires.

**Do not plug the dongle into a computer USB port.** The USB-A connector is only a shape: it carries UART, not USB. Do not
plug regular USB devices into the appliance port either: the voltage there differs.

## Wiring

Bracket cable colours vary, so check with a multimeter. USB-A pin 1 is at the edge, next to pin 2.

| Adapter | USB-A pin | Usual wire colour |
|---|---|---|
| 5V | 1 | Red |
| TX | 2 (next to power) | White |
| RX | 3 | Green |
| GND | 4 | Black |

If the dongle does not reply, swap TX and RX; this is safe. Apply power **after** the button step, so connect 5V last.

## Flashing

1. Download your program from [Releases](https://github.com/BUCKSN/esphome-dongle/releases):
   `esphome-dongle-flash-<system>-<arch>`, where system is `windows`, `macos` or `linux`, arch is `x64` (Intel, AMD) or
   `arm64` (Apple Silicon M1 and later, other ARM). On Linux and macOS run `chmod +x <file>`. Run it from your own folder: the backup is saved there.
2. Windows and macOS warn about an unsigned program. Check the checksum in `SHA256SUMS.txt` and allow it manually.
   The adapter may need its vendor driver.
3. Connect the adapter without the 5V wire, start the program and answer its questions.
4. When asked: hold the button on the dongle end, connect 5V, release the button, press Enter.
5. The program reads the full memory twice, about 12 minutes. Stop if the copies differ.
6. **Keep the `esphome-dongle-backup-...` folder.** It contains your personal token: do not publish it. Only it restores the stock firmware.
7. Confirm writing and wait for "Done".

To restore the stock firmware: run the program with `--restore folder/dump-1.bin`.

## Setup

1. Remove power and wires, plug the dongle into the appliance while it is off, then turn the appliance on.
2. Join the open `esphome-dongle-XXXXXX` network. The dongle page opens: choose your Wi-Fi and enter the password.
3. On first start the page stays open for an hour after connecting. Open `http://esphome-dongle-XXXXXX.local/` or the IP from your router. Later a short button press opens it.
4. Add the device to Home Assistant with the ESPHome integration. Do this on a trusted network; the encryption key is in the
   "Home Assistant key" section of the dongle page.

You can change Wi-Fi on the dongle page: if it fails, the dongle returns to the previous network in a minute.

Reset: hold the button. Red lights after 2 s (if LEDs are on); at 10 s it blinks and you can release. Wi-Fi and the key are erased. A new key is created on the next start.
