# AGENTS.md

## What this is

Single-target **Qt 6 Widgets** desktop app (`tomoss`) for Toomoss (图莫斯) USB2XXX adapters (CAN / LIN / PWM). App code lives under `src/`; vendored SDK under `3rdparty/usb2xxx/`; `build/` is Qt Creator output (gitignored).

| Path | Role |
|------|------|
| `src/main.cpp` | Entry: `QApplication` + `MainWindow` |
| `src/mainwindow.*` | Window class + Designer form |
| `3rdparty/usb2xxx/include/` | C API headers (`usb_device.h`, `usb2can*.h`, `usb2lin*.h`, `usb2pwm.h`, `offline_type.h`) |
| `3rdparty/usb2xxx/lib/windows/x86_64/` | `USB2XXX.lib` + runtime DLLs (`USB2XXX`, `libusb-1.0`, `binlog`) |
| `CMakeLists.txt` | Sole build definition |

SDK source of truth (upstream example repo): `usb2can_lin_pwm_example` — only the CAN/LIN/PWM-related headers and Windows x64 libs were copied in. Do not pull the full SDK tree (mac/linux/android, VB, docs).

## Build (exact)

Requires Qt **≥ 6.5** (Core + Widgets). Local kit: Qt **6.11.2** MinGW 64 at `C:/Applications/Qt/6.11.2/mingw_64`, Ninja.

```powershell
cmake -S . -B build/Desktop_Qt_6_11_2_MinGW_64_bit_Debug `
  -G Ninja `
  -DCMAKE_PREFIX_PATH=C:/Applications/Qt/6.11.2/mingw_64 `
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build/Desktop_Qt_6_11_2_MinGW_64_bit_Debug
```

POST_BUILD copies USB2XXX DLLs next to `tomoss.exe`. Link is hardcoded to `3rdparty/usb2xxx/lib/windows/x86_64/USB2XXX.lib` (MSVC import lib; works with this kit’s MinGW — if link fails after changing toolchain, regenerate an import lib from the DLL).

## Layout / build quirks

- New sources must be listed in `qt_add_executable(...)` with `src/` paths — no glob.
- Include path for the SDK is `3rdparty/usb2xxx/include` only; do not add ad-hoc `#include` paths into `build/`.
- **Do not edit** generated `ui_*.h` / `moc_*.cpp` under `build/**/tomoss_autogen/`. Edit `src/mainwindow.ui` or headers.
- `.gitignore` ignores `*.dll`/`*.exe` globally but **un-ignores** `3rdparty/**` so vendored libs stay in git.
- No Linux/mac SDK variants are vendored yet — Windows x64 only.
- Runtime needs DLLs beside the exe (handled by CMake POST_BUILD). `USB2XXX_EX.*` is not linked.

## Versioning (semver)

**Single source of truth:** `project(tomoss VERSION x.y.z)` in `CMakeLists.txt`.

- Exposed as `TOMOSS_VERSION` → `version::semver()` (`src/core/appversion.h`)
- Shown in window title and `QApplication::applicationVersion`
- Bump rules:
  - **PATCH** `z` — bugfix only
  - **MINOR** `y` — new feature, backward compatible (e.g. add CAN page)
  - **MAJOR** `x` — breaking UI/API or settings format change
- After bump: reconfigure CMake (`cmake -S . -B …`) so the define updates
- Release tags: `vX.Y.Z` on the commit that shipped the bump

Current: see `CMakeLists.txt` `project(... VERSION …)`.

## Tests / lint / CI

**None.** Verification = configure + link cleanly; optionally launch with device attached.

## Conventions

- Programmatic UI under `src/ui/*`; shell wiring in `MainWindow` (no `.ui` form).
- C API usage follows upstream examples: `#include "usb_device.h"` / `"usb2lin_ex.h"` etc.
- Scope: Qt Core+Widgets + USB2XXX C API only — do not add modules unless the task requires them.
