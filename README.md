# SJ-key - USB MIDI Keyboard Controller

**SJ-key ** is a mini USB MIDI keyboard controller based on the **RP2040-Zero** microcontroller. Featuring 4 main keys and 1 function switch key, it supports 4 MIDI Banks, EEPROM persistence, and integrated FastLED lighting effects.

---

## ✨ Features

* **4 MIDI Bank Switching**: Cycle through banks using the `SW_MODE` key to control a total of 16 MIDI notes (4 key ver.).
* **EEPROM Persistence**: Automatically saves and restores the current Bank state after powering off.
* **Rich RGB Lighting**: Supports per-key backlighting and onboard status LEDs, with dynamic color updates via MIDI Input.

---

## 🛠️ Hardware Specs & Pin Configuration (GPIO) (4 key ver.)

### 1. Button to GPIO Mapping
* **SW_MODE**: `GPIO 13` (Bank Switching)
* **SW1 ~ SW4**: `GPIO 10`, `GPIO 9`, `GPIO 11`, `GPIO 12`

### 2. RGB LED Pins
* **Onboard RGB LED (WS2812B)**: `GPIO 16`
* **Key Backlight (SK6812 MINI-E)**: `GPIO 28`

---

## 💻 Environment Setup & Flashing

1. **Board Settings**: In the Arduino IDE, select **Raspberry Pi Pico** as your board, and set the USB Stack to **Adafruit TinyUSB** (`Tools > USB Stack > Adafruit TinyUSB`).
2. **Dependencies**: Install the `Adafruit TinyUSB Library`, `Control Surface` (by tttapa), and `FastLED` libraries.
3. **Flashing**: Connect your device via USB and upload the sketch to start using it.
