# Ideas: non-voice projects for the M5StickC Plus

A pitch list for Colson to pick from. Nothing here is built or tested yet. Each idea uses only hardware already owned (M5StickC Plus 1.1, Plus 1.2, the e-ink kit, speakers, Pi Zero v1.3) and free software.

## What the stick has to work with

| Part | Plus 1.1 | Plus 1.2 (Plus2) |
| --- | --- | --- |
| IMU (accelerometer + gyro) | yes | yes |
| IR LED (transmit only, no receiver) | yes, G9 | yes, shared with the LED on G19 |
| Buzzer | yes | yes |
| Buttons | A, B, power | A, B, power |
| Screen | 135 x 240 colour LCD | 135 x 240 colour LCD |
| Wi-Fi + Bluetooth LE | yes | yes |
| RTC, battery | yes | yes |
| PSRAM | no | 2 MB |
| Free pins | Grove (G32/G33), hat header | Grove, hat header |

Pin numbers are from the vendor docs and should be checked on the board before wiring anything.

Effort is rough wall-clock time for a first working version: S is an evening, M is a weekend, L is several weekends.

## The ideas

### 1. Bluetooth macro remote for the Mac

The stick pairs with the Mac as a Bluetooth keyboard. Button A, button B, long presses and tilt gestures each send a shortcut you choose: next slide, play/pause, mute in a call, lock the screen, run a Shortcuts automation. The screen shows the current "page" of bindings, and a flick of the wrist switches pages.

- Hardware: buttons, IMU, BLE, screen.
- Software: ESP32 BLE Keyboard library, no Mac app needed.
- Effort: S. Wow: medium, and you would actually use it every day.

### 2. Air mouse and presenter

Same Bluetooth pairing, but as a mouse. Tilt to move the pointer, A to click, B to right-click, hold B and tilt to scroll. Doubles as a slide clicker with a laser-pointer style dot.

- Hardware: IMU, buttons, BLE.
- Effort: M (most of the time goes into smoothing and dead zones so the pointer does not drift). Wow: high.
- Builds directly on idea 1; the two could be one firmware with a mode switch.

### 3. Your own TV and AC remote

Learn the IR codes for your own TV, soundbar or air conditioner from public code databases (the stick can send IR but cannot record it), then put them on a menu: power, volume, input. Add a "bedtime" macro that turns everything off with one press, or a sleep timer that sends power-off in 30 minutes.

- Hardware: IR LED, buttons, screen, RTC.
- Software: IRremoteESP8266 library.
- Effort: S once the right codes are found. Wow: medium, very practical.
- Risk: IR range from the tiny LED is short, a few metres at best.

### 4. Flip-to-focus timer

A focus timer you start by putting the stick face down on the desk and stop by picking it up. The IMU knows which way up it is. It buzzes at the end of a block, shows a daily tally, and can (with idea 1's Bluetooth code) toggle Do Not Disturb on the Mac when a block starts.

- Hardware: IMU, buzzer, screen, RTC.
- Effort: S. Wow: medium. Good first project because it is small and complete.

### 5. Rep counter for workouts

Strap the stick to a wrist or thigh and it counts push-ups, squats or curls from the motion pattern, beeps each rep, and shows sets on screen. Start simple with peak counting on one axis; later, log sessions to a file on the Mac over Wi-Fi.

- Hardware: IMU, buzzer, screen, Wi-Fi (optional).
- Effort: M. Wow: high when it counts correctly.
- Risk: different exercises need different tuning; plan on one exercise first.

### 6. Pocket tilt game

An original game played by tilting the stick: roll a ball through a scrolling maze, dodge obstacles, collect dots. High score saved on the device, buzzer for sound effects. The 135 x 240 screen in portrait suits a vertical scroller.

- Hardware: IMU, screen, buzzer, buttons.
- Effort: M. Wow: high for showing friends.

### 7. Wi-Fi e-ink desk dashboard

The stick fetches weather, the date and a short to-do list over Wi-Fi and draws it on the 2.13" quad-colour e-ink panel, which keeps the image with no power. Refresh every 30 minutes and deep-sleep in between, so the battery lasts days.

- Hardware: Wi-Fi, e-ink panel via the Feather Friend, wired to the stick's free pins (SPI needs about five pins, which the Grove port plus hat header can just about cover).
- Software: free Open-Meteo weather API, GxEPD2 or Adafruit EPD library.
- Effort: L, mostly wiring and pin juggling. Wow: high, and it finally uses the e-ink kit.
- Cheaper variant to start: show the same dashboard on the stick's own LCD first (effort S), then move it to e-ink.

### 8. Walk-away Mac lock

The Mac watches for the stick's Bluetooth signal. When you walk away and the signal fades, a small script on the Mac locks the screen; when you come back, the stick shows "unlock" so you know it saw you. The stick side is tiny; most of the work is a Mac script that reads signal strength.

- Hardware: BLE, screen.
- Effort: M. Wow: medium-high.
- Risk: signal strength is noisy indoors, so it needs a threshold and a delay to avoid false locks.

### Honourable mentions

- **Spirit level and protractor**: live angle on screen with a beep at level. Effort S.
- **Wi-Fi signal mapper**: walk the house and see your own network's signal strength in each room to find dead spots. Effort S.
- **Desk clock with alarms**: RTC, buzzer, auto-dim at night. Effort S.

The Pi Zero v1.3 has no Wi-Fi, so none of these lean on it. It could later act as a wired USB host for a stick, but that is not worth planning around yet.

## Top 3 recommendation

1. **Bluetooth macro remote (idea 1)**. Smallest effort, daily use, and its Bluetooth keyboard code is the base for ideas 2, 4 and 8.
2. **Flip-to-focus timer (idea 4)**. A complete little product in one evening, shows off the IMU in a way that feels clever, and can reuse idea 1's code to set Do Not Disturb.
3. **Wi-Fi dashboard (idea 7), LCD first, e-ink later**. The bigger project of the three, and the one that puts the e-ink kit to use. Starting on the LCD keeps the first step small.

If you want the most impressive demo rather than the most useful tool, swap in the **air mouse (idea 2)** or the **tilt game (idea 6)**.

## How a pick becomes a build

Whichever one you choose gets its own folder (like `firmware/` for dictation), a short brief and design note in `docs/`, and a CI build, following the same rules as the existing firmwares in `AGENTS.md`.
