# mpc-gen1recomp

Native Pokémon Red, Blue, and Yellow runtime & player for the **Akai MPC Sample**.

Built with [mpc-ui-toolkit](https://github.com/garrettjwilke/mpc-ui-toolkit) for direct DRM framebuffer display, hardware MIDI control surface integration with velocity-sensitive RGB backlit pads, ALSA audio, and USB mass storage SD card sharing.

---

## Features

* **Complete Pad Mapping & Configuration**:
  * 32 Pad slots across **Bank A** (1–16) and **Bank B** (17–32), switchable via physical `PAD BANK` button or menu.
  * **Interactive 4x4 Visual Rebind Grid** modal accessible anytime via top button `B2`.
  * Dynamic **RGB LED feedback** on the physical MPC pads (Blue for D-pad, Green for A, Red for B, Amber for Start, Yellow for Select, Purple for Turbo, Cyan for System/Menu) with full-brightness glow on pad hit.
  * 4 Curated Presets: *Arcade (Default)*, *Ergonomic (2-Handed)*, *Game Boy Classic*, and *Speedrunner*.
  * Automatic JSON persistence for pad bindings (`user/bindings.json`) and settings (`user/settings.json`).
* **In-Game Menus & Overlay**:
  * `B1` (**GAME**): Resume, Rebind, Switch Game/ROM, Preset selector, Bank toggle, Palette style, Aspect scaling, Reset, Quit.
  * `B2` (**REBIND**): 4x4 visual pad grid remapping.
  * `B3` (**SETTINGS**): USB Mass Storage (SD Card), Volume control, Internal speaker mute, Turbo multiplier, Save config.
* **USB SD Mass Storage & User-Supplied ROMs**:
  * Decoupled clean build: release packages contain **no copyrighted ROMs**.
  * Built-in USB mass storage mode (`usb_sd.c`) allows connecting the MPC to your PC/Mac via USB cable to drag-and-drop your legal ROM files into `roms/`.
  * Automatic **on-device extraction screen** with animated progress gauge (`0%..100%`), stage status, and activity spinner.
* **Multi-ROM Support**:
  * Pokémon Red (`pokemon_red.gb`)
  * Pokémon Blue (`pokemon_blue.gb`)
  * Pokémon Yellow (`pokemon_yellow.gbc`)
* **Color Palettes & Display Options**:
  * Palettes: GBC Color, Super Game Boy (SGB), DMG Green (Original pea-soup), Pocket Gray (Monochrome).
  * Aspect scaling: 1x Centered (160x144), 1.5x Letterbox (240x216), Full Stretched (320x240).

---

## Controls

### Top App Bar Buttons
| Button | Mode | Description |
| :--- | :--- | :--- |
| **B1** | **GAME** | Game Menu (Resume, Rebind, Select Game, Preset, Bank, Palette, Quit) |
| **B2** | **REBIND** | Interactive 4x4 Pad Grid remapping modal |
| **B3** | **SETTINGS** | Settings Menu (USB Storage, Volume, Speaker, Turbo Speed, Save Config) |

### Default Pad Layout (Arcade Preset - Bank A)
| Physical Row | Pads | Default Functions |
| :--- | :--- | :--- |
| **Row 4 (Top)** | Pads 13, 14, 15, 16 | `MENU`, `SCREEN_MODE`, `QUICK_SAVE`, `QUICK_LOAD` |
| **Row 3** | Pads 9, 10, 11, 12 | `SELECT`, `START`, `TURBO` (toggle), `PALETTE` |
| **Row 2** | Pads 5, 6, 7, 8 | `B` (Cancel), `UP` (D-Pad), `A` (Confirm), `B` (Cancel) |
| **Row 1 (Bottom)** | Pads 1, 2, 3, 4 | `LEFT`, `DOWN`, `RIGHT`, `A` (Confirm) |

* Press the physical `PAD BANK` button to switch to **Bank B** (Pads 17–32) for alternate shortcuts and utilities.

---

## Installing ROMs via USB Storage

1. Boot `mpc-gen1recomp`. If no ROMs are detected, the app automatically enters **USB Storage** mode. (You can also enter it anytime via `B3` Settings -> *USB storage (SD card)*).
2. Connect a USB-C / micro-USB cable from the MPC Sample to your computer.
3. The MPC SD card will mount as a standard USB flash drive on your desktop.
4. Copy your legal ROM files into the `roms/` folder on the card:
   * `roms/pokemon_red.gb`
   * `roms/pokemon_blue.gb`
   * `roms/pokemon_yellow.gbc`
5. Eject the SD card safely on your computer.
6. Press `B1` or Push the rotary encoder on the MPC to exit USB storage.
7. The MPC will detect the newly added ROMs and display the **Importing Progress Screen**. Once extraction completes, the game launches immediately.

---

## Building & Deployment

### Requirements
* `zig` (0.17+ recommended)
* `python3` (for packing autorun archive)
* `cc` / `make`

### Build Host Test Suite
```bash
make test
```

### Build Device Package (ARM64 static binary)
```bash
make
```
This produces:
* `build/mpc-gen1recomp` (aarch64 static ELF binary)
* `build/autorun/` (ready-to-boot SD card folder)
* `build/mpc-gen1recomp.zip` (verified autorun package)

### Deploy to MPC via SSH
```bash
make deploy
```
Uses `mpc-boot-thingy` to upload and install over your local network.

