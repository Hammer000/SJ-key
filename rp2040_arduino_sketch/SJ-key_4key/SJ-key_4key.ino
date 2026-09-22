/* A MIDI controller that can send MIDI note via USB
 *
 * MCU: RP2040-Zero (Board set to Raspberry Pi Pico)
 * USB Stack:
 * Adafruit TinyUSB (Tools > USB Sack > Adafruit TinyUSB)
 * Dependencies:
 * FastLED
 * Control Surface by Pieter P (tttapa)
 */

#include <Adafruit_TinyUSB.h>
#include <Control_Surface.h>
#include <FastLED.h>
#include <EEPROM.h>

// Total Switch keys
#define SWITCH_NUM 5
// MIDI
#define MIDI_BANK 4         //How many MIDI banks
#define MIDI_BANK_SHIFT 12  // 12 semitones = 1 octave
// RP2040-Zero WS2812B settings
#define RP2040_LED_PIN 16         // RP2040 RGB LED using GPIO16
#define RP2040_NUMPIXELS 1        // only 1 LED
#define RP2040_LED_BRIGHTNESS 12  // LED brightness (0-255)
// keyboard SK6812 MINI-E LED settings
#define KEY_LED_PIN 28               // Key RGB using GPIO28
#define KEY_NUMPIXELS 5              // 5 LEDs
#define KEY_LED_BRIGHTNESS 5         // Normal LED brightness (0-255)
#define KEY_PRESS_LED_BRIGHTNESS 40  // Button press LED brightness (0-255)
#define DEFAULT_VELOCITY_127_COLOR  CRGB::White  // original 0x4B1502 

// Save/restore the bank number
#define EEPROM_BANK_ADDR 0  // Use EEPROM ADDR 0

uint8_t gRainbow_led_hue = 0;           // rainbow
uint8_t gCurrent_bank = 0;              // current MIDI bank
bool gMidi_bank_updated = 0;            // For EEPROM save check
bool gLast_sw_mode_btn_state = HIGH;    // Last SW_MODE button status
unsigned long gLast_debounce_time = 0;  // for debounce

// GPIO settings for each switch
enum SW_NUM {
  SW_MODE_GPIO = 13,  //SW_MODE connect to GPIO13
  SW1_GPIO = 10,      //SW1 connect to GPIO10
  SW2_GPIO = 9,       //SW2 connect to GPIO9
  SW3_GPIO = 11,      //SW3 connect to GPIO11
  SW4_GPIO = 12,      //SW3 connect to GPIO12
};
const int key_list[SWITCH_NUM] = { SW_MODE_GPIO, SW1_GPIO, SW2_GPIO, SW3_GPIO, SW4_GPIO };

//Order of key LED
enum SW_LED_ORDER {
  SW_MODE_LED = 0,  //First LED
  SW1_LED,          //2nd LED... MIDI start
  SW2_LED,
  SW3_LED,
  SW4_LED,
};

// Initial LED
const CRGB default_btn_press_color[KEY_NUMPIXELS] = { 0, CRGB::Red, CRGB::Green, CRGB::Blue, CRGB::Purple };
const CRGB default_bank_color[MIDI_BANK] = { CRGB::White, CRGB::Green, CRGB::Blue, CRGB::Purple };
CRGB rp2040_led[RP2040_NUMPIXELS];
CRGB key_leds[KEY_NUMPIXELS];
CRGB current_btn_press_color[KEY_NUMPIXELS];
CLEDController *rp2040_controllers;
CLEDController *keys_controllers;

// MIDI interface
USBMIDI_Interface midi;
Bank<MIDI_BANK> bank(MIDI_BANK_SHIFT);

// Define the MIDI note for each switch (same order as SW_MIDI_ORDER)
// {GPIO num, {note, channel}}
// Bank1 C0/D0/E0/F0
// Bank2 C1/D1/E1/F1
// ...
const uint8_t midi_base_note[SWITCH_NUM - 1] = { 24, 26, 28, 29 };
Bankable::NoteButton buttons[] = {
  { bank, SW1_GPIO, { midi_base_note[0], Channel_1 }, 0x7F },  // SW1
  { bank, SW2_GPIO, { midi_base_note[1], Channel_1 } },  // SW2
  { bank, SW3_GPIO, { midi_base_note[2], Channel_1 } },  // SW3
  { bank, SW4_GPIO, { midi_base_note[3], Channel_1 } },  // SW4
};
//Order of key MIDI (Same as NoteButton order)
enum SW_MIDI_ORDER {
  SW1_MIDI = 0,
  SW2_MIDI,
  SW3_MIDI,
  SW4_MIDI,

  MAX_MIDI_KEY
};

/****** Main code ******/

// velocity to color mapping table
const CRGB velocity_to_color[128]{
  0x000000, 0x1E1E1E, 0x7F7F7F, 0xFFFFFF, 0xFF4C4D, 0xFF0001, 0x590000, 0x190000, 0xFFBD6C, 0xFF5400,
  0x591D01, 0x271B00, 0xFFFF4C, 0xFFFF02, 0x595900, 0x191900, 0x88FF4B, 0x54FF00, 0x1D5900, 0x142B00,
  0x4CFF4C, 0x02FF01, 0x005900, 0x001900, 0x4CFF5E, 0x02FF1A, 0x00590D, 0x001902, 0x4CFF88, 0x01FF55,
  0x01591D, 0x001F12, 0x4DFFB7, 0x02FF99, 0x005935, 0x001912, 0x4CC3FF, 0x00A9FF, 0x004152, 0x001019,
  0x4C88FF, 0x0055FF, 0x001D59, 0x000819, 0x4C4CFF, 0x0000FF, 0x000058, 0x000019, 0x874CFF, 0x5300FF,
  0x190064, 0x0F0030, 0xFF4CFF, 0xFF00FF, 0x590059, 0x190019, 0xFF4C87, 0xFF0054, 0x59001D, 0x220013,
  0xFF1400, 0x993500, 0x993500, 0x436400, 0x033900, 0x005735, 0x00547F, 0x0000FF, 0x00454F, 0x2500CC,
  0x7F7F7F, 0x202020, 0xFF0001, 0xBDFF2D, 0xAFEC05, 0x64FF09, 0x108B01, 0x00FF87, 0x00A9FF, 0x002AFF,
  0x3F00FF, 0x7A00FF, 0xB21A7D, 0x402000, 0xFF4A01, 0x88E107, 0x72FF16, 0x02FF01, 0x3BFF25, 0x59FF71,
  0x39FFCC, 0x5B8AFF, 0x3151C6, 0x877FE8, 0xD31DFF, 0xFF005D, 0xFF7F00, 0xB9B000, 0x90FF01, 0x825C07,
  0x392B00, 0x144C10, 0x0D5038, 0x15152A, 0x16205A, 0x693C1C, 0xA8000A, 0xDE513D, 0xD86A1C, 0xFFE126,
  0x9EE02F, 0x67B50F, 0x1E1E2F, 0xDCFF6A, 0x80FFBD, 0x9A99FF, 0x8E66FF, 0x404040, 0x757575, 0xE0FFFF,
  0xA00001, 0x350000, 0x1AD000, 0x074200, 0xB9B000, 0x3F3100, 0xB35F00, DEFAULT_VELOCITY_127_COLOR
};

// Save current MIDI bank number to EEPROM
void update_eeprom_midi_bank() {
  if (gMidi_bank_updated) {
    // Write current MIDI bank to EEPROM
    EEPROM.write(EEPROM_BANK_ADDR, gCurrent_bank);
    EEPROM.commit();

    gMidi_bank_updated = false;
  }
}

// check SW_MODE button state
void check_sw_mode_state() {
  bool sw_mode_status = digitalRead(SW_MODE_GPIO);

  if (sw_mode_status != gLast_sw_mode_btn_state) {
    gLast_debounce_time = millis();
    gLast_sw_mode_btn_state = sw_mode_status;
  }

  if ((millis() - gLast_debounce_time) > 50) {  //debounce
    static bool stable_btn_state = HIGH;

    // stable state changed
    if (sw_mode_status != stable_btn_state) {
      stable_btn_state = sw_mode_status;

      // only trigger while button falling
      if (stable_btn_state == LOW) {
        uint8_t nextBank = (bank.getSelection() + 1) % MIDI_BANK;

        // update MIDI bank
        bank.select(nextBank);
      }
    }
  }
}

// self defined callback function to get MIDI in messages to change LED color
class SJMIDICallbacks : public MIDI_Callbacks {
public:
  void onChannelMessage(MIDI_Interface &midi, ChannelMessage msg) override {
    uint8_t note = msg.data1;
    uint8_t velocity = msg.data2;  // color data
    uint8_t current_bank_offset = MIDI_BANK_SHIFT * bank.getSelection();

    if (msg.getMessageType() == MIDIMessageType::NoteOn) {  //Note On msg
      for (int i = 0; i < MAX_MIDI_KEY; i++) {
        if (midi_base_note[i] + current_bank_offset == note) {
          current_btn_press_color[SW1_LED + i] = velocity_to_color[velocity];
        }
      }
    } else if (msg.getMessageType() == MIDIMessageType::NoteOff) {  //Note Off msg
      for (int i = 0; i < MAX_MIDI_KEY; i++) {
        if (midi_base_note[i] + current_bank_offset == note) {
          current_btn_press_color[SW1_LED + i] = velocity_to_color[velocity];
        }
      }
    }
  }
};
SJMIDICallbacks midi_callback;

// Check SW status and update RGB LED color
void update_key_led_color() {
  uint8_t led_brightness = KEY_LED_BRIGHTNESS;
  uint8_t key_led_brightness[KEY_NUMPIXELS];
  bool key_press = false;
  bool midi_msg_in = false;

  if (KEY_LED_BRIGHTNESS <= 0)
    return;

  // set default brightness
  memset(key_led_brightness, KEY_LED_BRIGHTNESS, sizeof(key_led_brightness));

  // Update rainbow RGB LED color first
  EVERY_N_MILLISECONDS(20) {
    gRainbow_led_hue++;
  }
  fill_rainbow(key_leds, KEY_NUMPIXELS, gRainbow_led_hue, 10);

  for (int i = 0; i < MAX_MIDI_KEY; i++) {
    if (current_btn_press_color[SW1_LED + i] != 0) {
      key_leds[SW1_LED + i] = current_btn_press_color[SW1_LED + i];
      key_led_brightness[SW1_LED + i] = KEY_PRESS_LED_BRIGHTNESS;
      midi_msg_in = true;
      key_press = true;
    }
  }

  if (!midi_msg_in) {
    // No MIDI msg received use default color if button pressed
    for (int i = 0; i < MAX_MIDI_KEY; i++) {
      if (buttons[i].getButtonState() == AH::Button::Pressed) {
        key_leds[SW1_LED + i] = default_btn_press_color[SW1_LED + i];
        key_led_brightness[SW1_LED + i] = KEY_PRESS_LED_BRIGHTNESS;
        key_press = true;
      }
    }
  }

  if (key_press) {
    // Set single LED brightness need nscale8() + max brightness with showLeds()
    for (int i = 0; i < KEY_NUMPIXELS; i++) {
      key_leds[i].nscale8(key_led_brightness[i]);
    }

    keys_controllers->showLeds(255);
  } else {
    // No key press
    keys_controllers->showLeds(KEY_LED_BRIGHTNESS);
  }
}

void update_rp2040_led_color(bool force_update) {
  if (RP2040_LED_BRIGHTNESS <= 0)
    return;

  if (gCurrent_bank != bank.getSelection() || force_update) {
    gCurrent_bank = bank.getSelection();
    gMidi_bank_updated = true;

    switch (bank.getSelection()) {
      case 0:
        rp2040_led[0] = default_bank_color[0];  // Bank 1
        break;
      case 1:
        rp2040_led[0] = default_bank_color[1];  // Bank 2
        break;
      case 2:
        rp2040_led[0] = default_bank_color[2];  // Bank 3
        break;
      case 3:
        rp2040_led[0] = default_bank_color[3];  // Bank 4
        break;
    }
    // Update RP2040 LED
    rp2040_controllers->showLeds(RP2040_LED_BRIGHTNESS);
  }
}

void update_led_color(bool force_update) {
  update_key_led_color();
  update_rp2040_led_color(force_update);
}

void setup() {
  // Delay recommended for RP2040
  delay(100);

  // EEPROM
  EEPROM.begin(256);
  gCurrent_bank = EEPROM.read(EEPROM_BANK_ADDR);
  if (gCurrent_bank >= MIDI_BANK) {
    gCurrent_bank = 0;
  }

  // Set keys to input & pullup
  for (int i = 0; i < SWITCH_NUM; i++) {
    pinMode(key_list[i], INPUT_PULLUP);
  }

  // Set MIDI device name
  USBDevice.setManufacturerDescriptor("Hammer000");
  USBDevice.setProductDescriptor("SJ-key (4key)");
  // Initial MIDI Control Surface
  midi.setCallbacks(midi_callback);
  Control_Surface.begin();
  bank.select(gCurrent_bank);

  // Initial and turn off RGB LED
  rp2040_controllers = &FastLED.addLeds<WS2812B, RP2040_LED_PIN, GRB>(rp2040_led, RP2040_NUMPIXELS);
  keys_controllers = &FastLED.addLeds<WS2812B, KEY_LED_PIN, GRB>(key_leds, KEY_NUMPIXELS);

  FastLED.clear();
  update_led_color(true);
}

void loop() {
  // polling SW status and send out the MIDI
  Control_Surface.loop();
  // check SW_MODE button state
  check_sw_mode_state();
  // update LED color
  update_led_color(false);
  // Save to EEPROM if MIDI bank changed (There's no actual EEPROM on RP2040, add delay)
  EVERY_N_SECONDS(5) {
    update_eeprom_midi_bank();
  }

  delay(2);
}