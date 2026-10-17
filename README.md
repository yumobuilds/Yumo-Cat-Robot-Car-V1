# 🐱 Yumo Cat Car

A desk-pet robot car with an animated cat face, plus a joystick remote.

The car has a round TFT screen that plays frame-by-frame cat animations (8 emotions), two small OLED "number plates" that show text and effects, and four continuous-rotation servos for wheels. It reacts to touch, wakes up with a little story when powered on, falls asleep when ignored, and can be driven with a wireless joystick remote over ESP-NOW.

Built by **[Yumo Builds](https://www.youtube.com/@YumoBuilds)**. 3D printing and design experiments live on **[Yumo Builds Lab](https://www.youtube.com/@YumoBuildsLab)**.

> 🎥 **Build video:** coming soon on [Yumo Builds](https://www.youtube.com/@YumoBuilds)

<!-- Add a photo or GIF of the finished car here -->
<!-- ![Yumo Cat Car](images/yumo-cat-car.jpg) -->

---

## Features

- **Animated cat face** on a round 240×240 GC9A01 display, with 8 emotions: Idle, Happy, Surprised, Curious, Love, Sleepy, Angry and Sad
- **Face and wheels move together**: every emotion has its own wheel dance (sway, jolt, look-around, twerk, rock, and more)
- **Neon "YUMO BUILDS" splash** on boot, then a scripted wake-up sequence
- **Touch reactions**: each touch plays the next reaction in a cycle, so it never repeats the last one
- **Falls asleep** after 15 seconds of no activity, and wakes on touch or joystick movement
- **Two OLED plates** with text such as `WEEE!`, `HISS!` and `PURRR <3`, a heartbeat while idle, and rising `ZZZ` while asleep
- **Wireless joystick remote** with automatic pairing, proportional speed and mixed steering
- **Safety stop**: the car stops its wheels if the remote goes quiet
- **Custom 3D-printed wheels** (STL included)

---

## What it does

### Power on
1. A neon **YUMO BUILDS** sign flickers on the round screen.
2. The cat wakes up: Sleepy, Surprised, Curious, Happy (drive forward, back, then a full spin), Happy (twerk) and Love (gentle rock).
3. It settles into **Idle**. The startup sequence plays only once per power-up.

### Touch reactions
While Idle, each touch plays the next reaction from this list, then returns to Idle:

| Reaction | Plate text |
|---|---|
| Surprised | `!!` (flashing) |
| Curious | `MRRP?` |
| Happy | `WEEE!` |
| Love | `PURRR <3` |
| Angry | `HISS!` |
| Sad | `MEW...` |

Sleepy is not a reaction. The cat only falls asleep after 15 seconds without touch or joystick activity, and a touch while asleep wakes it up without using up a reaction.

---

## Parts list

The car and the remote each have their **own ESP32-C3 Super Mini**.

| Part | For | Qty | Notes |
|---|---|---|---|
| ESP32-C3 Super Mini | Car | 1 | Runs `Yumo_Cat_Bot_Car.ino` |
| ESP32-C3 Super Mini | Remote | 1 | Runs `Yumo_Cat_Bot_Remote.ino` |
| GC9A01 round TFT, 240×240, SPI | Car | 1 | The cat's face |
| SSD1306 OLED 128×32, I²C | Car | 2 | Addresses `0x3C` and `0x3D` (set one by the address jumper) |
| Touch sensor module (digital output) | Car | 1 | Pin `GPIO 4` |
| Continuous-rotation servos | Car | 4 | 1500 µs = stop. Model: `[fill in]` |
| Analog joystick module (VRx, VRy, SW) | Remote | 1 | |
| 18650 lithium battery (3.7 V) | Car | 2 | Power for the car |
| Small lithium battery (3.7 V) | Remote | 1 | Power for the remote |
| 3D-printed wheels | Car | 4 | PLA, see [3D printing](#3d-printing) |

---

## Wiring

### Car board (ESP32-C3 Super Mini)

| Function | GPIO |
|---|---|
| TFT backlight (BLK) | 2 |
| TFT DC | 3 |
| Touch sensor | 4 |
| TFT RST | 5 |
| TFT SCLK | 6 |
| TFT MOSI | 7 |
| OLED SDA (I²C) | 8 |
| OLED SCL (I²C) | 9 |
| TFT CS | 10 |
| Servo front-left | 0 |
| Servo back-left | 1 |
| Servo back-right | 20 |
| Servo front-right | 21 |

### Remote board (ESP32-C3 Super Mini)

| Function | GPIO |
|---|---|
| Joystick VRx | 3 |
| Joystick VRy | 1 |
| Joystick button (SW) | 0 |
| Status LED | 8 (onboard, active LOW) |

> ⚠️ **Power the joystick from 3.3 V, not 5 V.** The ESP32 analog pins are 3.3 V only.

The remote's code swaps the two axes in software because of how the joystick module is mounted. If your stick feels rotated or backwards, change `PIN_JOY_X`, `PIN_JOY_Y`, `INVERT_X` and `INVERT_Y` at the top of `Yumo_Cat_Bot_Remote.ino`.

---

## Software setup

The car and the remote each have their **own ESP32-C3 Super Mini** and run their own sketch, so set up the Arduino IDE for each board separately.

### Build configuration

> ⚠️ **Use exactly these versions.** A sketch that compiles on one computer can fail on another when the ESP32 core or a library is a different version. If something does not compile or behave correctly, install the versions below first.

#### Tested with

Arduino IDE: `2.0.18`

| Item | Car board | Remote board |
|---|---|---|
| Sketch | `Yumo_Cat_Bot_Car.ino` | `Yumo_Cat_Bot_Remote.ino` |
| ESP32 board package (Arduino core) | `2.0.18-arduino.5` | `[fill in]` |
| Extra libraries | 5 (listed below) | None |

To install a specific version, open **Tools → Board → Boards Manager** (or **Sketch → Include Library → Manage Libraries**), search for the item and pick the version from the drop-down before pressing Install.

#### Libraries (car board only)

| Library (install name in Library Manager) | Version | Author | Used for |
|---|---|---|---|
| GFX Library for Arduino | `1.6.9` | moononournation | GC9A01 round display |
| Adafruit SSD1306 | `2.5.17` | Adafruit | The two OLED plates |
| Adafruit GFX Library | `1.12.6` | Adafruit | Text and fonts on the OLEDs |
| TJpg_Decoder | `1.1.0` | Bodmer | Decoding the JPEG animation frames |
| ESP32Servo | `3.2.1` | Kevin Harrington / Dlloydev | Servo control |

`Wire`, `WiFi`, `esp_now` and `esp_wifi` come with the ESP32 core. The remote needs no extra libraries.

#### Board settings (Arduino IDE)

| Setting | Car board | Remote board |
|---|---|---|
| Board | ESP32C3 Dev Module | ESP32C3 Dev Module |
| Flash size | 4 MB | `[fill in]` |
| Partition scheme | Huge APP | `[fill in]` |

Why the car board needs *Huge APP*: the 120 animation frames add up to about **1.5 MB** of flash, which does not fit in the default app partition. The remote sketch has no animation data, so it is much smaller.

### Upload
1. Open `car/Yumo_Cat_Bot_Car/Yumo_Cat_Bot_Car.ino` in the Arduino IDE and upload it to the car's board. The `*_frames.h` animation files in the same folder load automatically.
2. Open `remote/Yumo_Cat_Bot_Remote/Yumo_Cat_Bot_Remote.ino` and upload it to the remote's board.
3. Power both on. They pair automatically.

---

## How the remote works

- The remote broadcasts a pair request every 250 ms on **ESP-NOW channel 1** until the car answers. The car replies and the remote remembers its MAC address.
- Once connected, the remote sends joystick data **50 times per second**, even when the stick is centred. The car sends a heartbeat back about 10 times per second.
- If the remote hears no heartbeat for **1 second**, it drops the link and goes back to searching.
- The car stops its wheels after **300 ms** without a packet.
- Only one remote controls the car at a time. A second remote can take over only after the first one has gone quiet.
- When paired, the car's OLED plates flash **REMOTE FOUND**, then **REMOTE CONNECTED**.

### Remote status LED

| LED | Meaning |
|---|---|
| Even fast blink | Searching for the car |
| Mostly dark, short blip every second | Radio failed to send |
| Solid on | Connected |

### Driving
- Push the stick forward or back to drive, and left or right to turn.
- Speed is **proportional**: a small push is slow, a full push is fast, with a curve for finer control at low speed.
- Forward and turning are mixed, so a diagonal push drives in a curve.
- A deadzone around the centre prevents drift.
- Driving counts as activity, so the cat will not fall asleep while you drive. Moving the stick also wakes a sleeping cat.
- The joystick drives the car only while it is in **Idle**. During the startup sequence or a touch reaction the animation has control, and driving resumes afterward.
- The joystick button is transmitted, but no function is assigned to it yet.

### Packet format

Both sketches share this 10-byte packet, and it must match on both boards:

```cpp
struct __attribute__((packed)) YumoPacket {
  uint8_t id[4];   // "YUMO"
  uint8_t type;    // 1 pair request, 2 pair ack, 3 joystick, 4 link ack
  uint8_t button;  // 1 = pressed
  int16_t x;       // 0..4095 raw ADC
  int16_t y;       // 0..4095 raw ADC
};
```

---

## Animation files

Each `*_frames.h` file holds JPEG frames (240×240) stored in flash, which the car decodes and draws one after another.

| File | Emotion | Frames |
|---|---|---|
| `cat_frames.h` | Idle | 36 |
| `happy_frames.h` | Happy | 12 |
| `surprised_frames.h` | Surprised | 12 |
| `curious_frames.h` | Curious | 12 |
| `love_frames.h` | Love | 12 |
| `sleepy_frames.h` | Sleepy | 12 |
| `angry_frames.h` | Angry | 12 |
| `sad_frames.h` | Sad | 12 |

Playback speed is set per emotion in the `faces[]` table in the car sketch (for example Idle 12.5 fps, Surprised 24 fps, Sleepy 3 fps). To add your own emotion, generate a new frames file in the same format and add an entry to the `Emotion` enum and `faces[]`.

---

## Tuning

These constants are in `Yumo_Cat_Bot_Car.ino`:

| Constant | What it does |
|---|---|
| `SIDES_MIRRORED` | Set `true` if the right-side servos are mounted mirrored. If the car drives straight instead of turning, flip this. |
| `SERVO_TURN_FWD` / `SERVO_TURN_REV` | Turn speed. Slower grips the desk better. |
| `HAPPY_TURN_MS` | How long the Happy spin turns. Re-time it until the car makes one full turn. |
| `HAPPY_FWD_MS` / `HAPPY_BACK_MS` | Forward and backward distance in the Happy move. Keep them equal. |
| `IDLE_HOLD_BEFORE_SLEEP_MS` | Time before the cat falls asleep (15000 = 15 s). |
| `JOYSTICK_DEADZONE` | Stick dead area around the centre. |
| `REMOTE_TURN_SCALE` | How strong steering is compared to driving. |
| `REMOTE_SPEED_CURVE` | 1.0 = linear. Higher gives finer control at low speed. |
| `REMOTE_ANALOG_DRIVE` | Set `false` for simple fixed-speed driving. |

---

## 3D printing

The wheels were designed by me and printed on a **Bambu Lab H2C**.

| Item | Value |
|---|---|
| Project name | Micro servo wheel |
| Plate | Plate 1, 4 wheels per plate (one per servo) |
| Printer | Bambu Lab H2C |
| Build plate | Textured PEI plate |
| Filament | PLA, rainbow, 22.03 g (right nozzle) |
| Print time | about 58 minutes |


STL file: [`stl/Micro servo wheel.stl`](stl/Micro%20servo%20wheel.stl)

---

## Repository layout

```
Yumo-Cat-Robot-Car-V1/
├── car/
│   └── Yumo_Cat_Bot_Car/
│       ├── Yumo_Cat_Bot_Car.ino          car firmware
│       └── *_frames.h                    8 animation files
├── remote/
│   └── Yumo_Cat_Bot_Remote/
│       └── Yumo_Cat_Bot_Remote.ino       joystick remote firmware
├── stl/
│   └── Micro servo wheel.stl             wheel for the servos
├── .gitignore
├── LICENSE.md
└── README.md
```

The Arduino IDE needs each `.ino` file to sit in a folder with the same name, so please don't rename those two folders.

---

## Troubleshooting

- **Sketch too large:** choose a larger app partition (see [Board settings](#board-settings-arduino-ide)).
- **Compile errors that mention ESP-NOW, `esp_now_recv_info_t`, or a library function:** your ESP32 core or library version differs from the [tested versions](#tested-with). Install the exact versions listed.
- **Remote stays in "searching":** check that both boards use `YUMO_ESPNOW_CHANNEL 1` and the same packet definition, and that the car is powered on and has finished booting.
- **Car drives straight when it should turn (or the turn goes the wrong way):** change `SIDES_MIRRORED`, or swap the turn pulses.
- **Stick directions feel wrong:** flip `INVERT_X` / `INVERT_Y` or swap the axis pins in the remote sketch.
- **Spin does not complete one full turn:** adjust `HAPPY_TURN_MS`.

---

## License

This project is licensed under [PolyForm Noncommercial 1.0.0](LICENSE.md).

In short: you're free to use, modify and share this code and these files (and build your own Yumo Cat Car) for any noncommercial purpose, such as personal, hobby, educational or research use. You can't sell it, or a product built from it, without permission. Commercial licensing is available on request: reach out via [github.com/yumobuilds](https://github.com/yumobuilds). Credit is appreciated. Tag **@yumobuilds** if you build one!

*(This covers the code and files themselves, not the general idea. Copyright doesn't extend to concepts, only to a specific implementation.)*

The libraries listed above belong to their respective authors and are used under their own licenses.

---

## Follow the build

- 🔧 **Yumo Builds**: electronics, ESP32 and Arduino projects, built while learning: https://www.youtube.com/@YumoBuilds
- 🖨️ **Yumo Builds Lab**: 3D printing, electronics and creative design: https://www.youtube.com/@YumoBuildsLab
