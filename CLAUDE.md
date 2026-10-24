# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

InfiniTime is C++ firmware for the PineTime smartwatch (nRF52832, 64 KB RAM, 512 KB internal flash + 4 MB external SPI flash), built on FreeRTOS, LVGL 7 (UI) and NimBLE (Bluetooth).

## AI usage policy (read first)

`AI_USAGE.md` is binding for contributions to this project. In particular:

- Commit messages, PR descriptions, issue comments, documentation and code comments meant for humans must be written by the human contributor, not generated. Leave these for the user to write (or clearly mark drafts as AI-generated so they can rewrite them).
- Commit messages must not contain AI attribution: no `Co-Authored-By: Claude ...` or similar lines.
- AI must not be used for PR review, and the contributor must understand all code they submit.

## Build

There are no host-side unit tests. "Testing" means building the firmware, running in the [InfiniSim](https://github.com/InfiniTimeOrg/InfiniSim) simulator, or flashing a PineTime.

Submodules (`src/libs/lvgl`, `littlefs`, `arduinoFFT`) are required: `git submodule update --init`.

Easiest path is the Docker image, which bundles ARM GCC 15.3, nRF5 SDK 15.3.0, MCUBoot, adafruit-nrfutil and lv_font_conv. Outputs go to `build/output`:

```sh
docker run --rm -it -v ${PWD}:/sources --user $(id -u):$(id -g) infinitime/infinitime-build                          # all targets + DFU
docker run --rm -it -v ${PWD}:/sources --user $(id -u):$(id -g) infinitime/infinitime-build /opt/build.sh pinetime-app # single target
```

Native build:

```sh
cmake -S . -B build -DARM_NONE_EABI_TOOLCHAIN_PATH=... -DNRF5_SDK_PATH=... [-DBUILD_DFU=1] [-DBUILD_RESOURCES=1]
cmake --build build -j4 --target pinetime-app
```

- Targets: `pinetime-app` (standalone), `pinetime-mcuboot-app` (with bootloader, used for OTA/DFU), plus `pinetime-recovery`, `pinetime-recovery-loader` and their `-mcuboot-` variants.
- `CMAKE_BUILD_TYPE` defaults to Release. A full Debug build usually does not fit in flash; instead switch a single CMake target's RELEASE options to `${DEBUG_FLAGS}` in `src/CMakeLists.txt`.
- `-DENABLE_USERAPPS="Apps::Alarm, Apps::Timer, ..."` and `-DENABLE_WATCHFACES="WatchFace::Digital, ..."` select what's compiled in (order = launcher order). Defaults live in `src/displayapp/apps/CMakeLists.txt`. These are CMake cache variables, so reconfigure from a clean build dir when changing them.
- `-DTARGET_DEVICE=` selects hardware variant (`PINETIME` default; see `src/drivers/PinMap.h`).
- Flash and RAM are tight. CI posts text/data/bss size diffs against `main` on every PR; keep size growth in mind.

Simulator build (as in CI, needs SDL2, libpng, lv_font_conv):

```sh
git clone https://github.com/InfiniTimeOrg/InfiniSim.git && git -C InfiniSim submodule update --init lv_drivers
cmake -G Ninja -S InfiniSim -B build_lv_sim -DInfiniTime_DIR="$PWD" && cmake --build build_lv_sim
```

## Formatting and lint

- `clang-format` (v14+, `.clang-format`) is enforced in CI on changed `.cpp`/`.h` files outside `src/libs/` and `src/FreeRTOS/`. Format changed files with `git clang-format --style file`, or install the hook: `git config --local core.hooksPath hooks`.
- `clang-tidy` (`.clang-tidy`) runs in CI against the build's compile database (`clang-tidy -p build <file>`), informational only.
- `tests/test-format.sh` and `tests/test-tidy.sh` are CI scripts (they require `GITHUB_BASE_REF`), not local test runners.

## Coding conventions (`doc/coding-convention.md`)

- 2-space indent, `#pragma once`, `nullptr`, primary operator spellings (no `and`/`or`/`not`).
- PascalCase for classes, namespaces and methods; camelCase for variables, with no `m_`/`_` member prefixes or suffixes.
- Project includes use quotes and are rooted at `src/` (`#include "displayapp/screens/Screen.h"`), never `../`. External/std headers use `<>`.
- Use `auto` sparingly: not for fundamental or fixed-width integer types, except when initializing from a cast.
- Commit format: `module: Short description` + body. Fixup commits are squashed, conflicts resolved by rebase.
- Never modify vendored code under `src/libs/` or `src/FreeRTOS/`.

## Architecture

### Tasks and messaging

`main.cpp` statically constructs all drivers and controllers (no heap allocation for these) and starts `SystemTask`. `SystemTask::Work()` (the "MAIN" task, `src/systemtask/`) initializes hardware, owns the sleep/wake state machine and handles button, touch IRQ, charging and BLE events. It starts the `DisplayApp` task (`src/displayapp/DisplayApp.cpp`), which owns LVGL, the screen and the currently running app. Other tasks: NimBLE ("ll", "ble") and `HeartRateTask`.

Tasks communicate via FreeRTOS queues of small enums: `System::Messages` (`src/systemtask/Messages.h`, sent with `SystemTask::PushMessage`) and `Display::Messages` (`src/displayapp/Messages.h`, sent with `DisplayApp::PushMessage`). New cross-task events typically mean adding an enum value and handling it in the receiving task's loop. Sleep inhibition from apps uses `WakeLock` (`src/systemtask/WakeLock.h`).

### Controllers (`src/components/`)

Singleton-style service objects (DateTime, Battery, Settings, Motor, Alarm, Timer, FS, Ble, etc.) shared by reference. BLE GATT services and the NimBLE glue live in `src/components/ble/` (`NimbleController` registers all services). `FS` wraps LittleFS on the external SPI flash.

`Settings` persists a raw `SettingsData` struct to `/settings.dat`. Loading discards the file if `version` differs, so **bump `settingsVersion` in `src/components/settings/Settings.h` whenever `SettingsData`'s layout changes**.

### Apps and watch faces (`src/displayapp/`)

Every UI screen is a subclass of `Screens::Screen` (`screens/Screen.h`), created on launch and destroyed on exit. The constructor builds the LVGL UI, the destructor must clean it up (`lv_obj_clean(lv_scr_act())`) and undo side effects. Periodic updates use an `lv_task` calling `Refresh()`. Apps override `OnButtonPushed` / `OnTouchEvent` to consume input.

- **System apps** (launcher, settings, notifications...) are constructed explicitly in the switch in `DisplayApp::LoadScreen()`.
- **User apps** and **watch faces** are registered at compile time. Each defines an `AppTraits<Apps::X>` / `WatchFaceTraits<WatchFace::X>` specialization (in its header) with a `Create(AppControllers&)` factory; `UserApps.h` builds the `userApps` / watch-face arrays via `consteval` functions from the CMake-generated list. `AppControllers` (`Controllers.h`) is the bundle of references available to factories.

Adding a user app or watch face touches (see the "Add Legend watch face" commit for a template):
1. `src/displayapp/screens/<Name>.{h,cpp}` with the Traits specialization.
2. Add the `.cpp` to the source list in `src/CMakeLists.txt`.
3. Add the enum value in `src/displayapp/apps/Apps.h.in` (the template that CMake turns into `Apps.h`).
4. Add it to the defaults in `src/displayapp/apps/CMakeLists.txt` if it should be built by default.
5. Include the header in `src/displayapp/UserApps.h`.

Follow `doc/ui_guidelines.md` for UI (240x240 display, large touch targets).

### Fonts, symbols and external resources

- Built-in fonts are generated from `src/displayapp/fonts/fonts.json` by lv_font_conv at build time. Icons are glyphs from Font Awesome / Material Symbols: add the codepoint to the font's range in `fonts.json` and its UTF-8 string to `src/displayapp/screens/Symbols.h`.
- Large fonts and images used by some watch faces are **external resources** (`src/resources/fonts.json`, `images.json`), packaged into `infinitime-resources-*.zip` (`-DBUILD_RESOURCES=1`) and uploaded to the external flash via BLE. A watch face that depends on them implements `IsAvailable(FS&)` to check the files exist. Retired resource files go in `src/resources/obsolete_files.json`.

### Bootloader and recovery

`bootloader/README.md` covers MCUBoot, OTA and image validation. `recoveryLoader.cpp` and `DisplayAppRecovery.cpp` are the minimal recovery firmware variants. The firmware must be validated after OTA (`FirmwareValidation` screen) or the bootloader reverts it.

## Further docs

`doc/code/Intro.md`, `doc/code/Apps.md`, `doc/ble.md` (BLE API), `doc/MemoryAnalysis.md`, `doc/InfiniTimeVision.md` (feature scope), `doc/maintainer-guide.md`.
