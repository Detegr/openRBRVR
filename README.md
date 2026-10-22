# openRBRVR

[🇬🇧](README.md) - [🇨🇿](README_CZ.md) - [🇫🇷](README_FR.md)

![openRBRVR logo](img/openRBRVR.png)

Open source VR plugin for Richard Burns Rally.

## Features

- Performance overall seems better compared to RBRVR. On some stages the
  difference is huge, on others they're quite similar.
- The image is more clear with openRBRVR with same resolution compared to
  RBRVR.
- Vulkan backend via dxvk ([fork](https://github.com/TheIronWolfModding/dxvk)
  originally by TheIronWolf that adds D3D9 VR support).
- PaceNote plugin UI works correctly.
- Gaugerplugin works correctly (but has a performance impact).
- Supports both OpenXR and OpenVR.

## Installation instructions

This plugin can be installed with the official [RSF](https://rallysimfans.hu)
installer.

To install a more recent version than what is available in the RSF installer,
download the latest release and copy&paste all files into RBR folder,
overwriting existing files when asked to. Make sure that the RSF launcher has
Virtual Reality and openRBRVR enabled when manually installing new versions of
the plugin.

## Setup

Plugin settings can be changed from `Options -> Plugins -> openRBRVR` and saved
to `Plugins/openRBRVR.toml` via the menu.

### Experimental hand tracking

Available from 2.3.0-beta.1. Requires OpenXR and a runtime that supports hand
tracking. Currently tested with Quest 3; other headsets have not been tested.

Enable hand tracking through the OpenXR plugin menu and save the settings,
or edit the existing entries in `Plugins/openRBRVR.toml`:

```toml
runtime = 'openxr'

[OpenXR]
handTracking = true
```

With Virtual Desktop, enable **Settings -> Streaming -> Advanced Options ->
Forward tracking data to PC** in the Quest app.

The hand menu and placeable buttons require **2.3.0-beta.2** or later.

- To open menu: face left palm, do a pistol grip, pinch thumb and index finger.
- Place Start engine and Call for help buttons with the right index fingertip,
  then lock each position using the left index. Touching Start holds ignition
  until withdrawal; Call for help requires a continuous two-second touch.
- Save locked button placements automatically per car and restore them in later
  sessions. Buttons are visible while the menu is open and work while hidden.


Hands appear while driving in the cockpit. The glove models and texture
are embedded in the plugin DLL. **Show hands** (`handTracking = true`, default
off) enables the gloves, menu/icon and placed buttons together. To open the panel, face the left
palm toward the headset, release then pinch thumb/index to open, and touch a
button with either index fingertip. **Place start button** positions a cube on
the right index; touch the item again with the left index to lock it, then
withdraw and touch the cube to hold ignition down. Removing the fingertip
releases ignition. The cube is visible only with the menu open and works while
hidden too. Both button placements save automatically per car when locked and
restore in later sessions. Close or another
left-hand gesture dismisses the panel.
**Place call for help** places a separate cube the same way. Touch that cube
with either index for two seconds to trigger help once; lifting early cancels
the countdown. It also works while hidden. A progress bar shows the hold duration.
This requires focused cockpit driving and valid tracking of both hands.
Only the local palm-facing pinch is supported; no extra menu settings are needed.
If it conflicts with Quest system UI, turn off Show hands; system gestures
cannot be intercepted or suppressed.
See [hand menu settings and the headset checklist](HAND_TRACKING.md#small-hand-menu).

## Frequently asked questions

- See the [FAQ](https://github.com/Detegr/openRBRVR/blob/master/FAQ.md).

## Build instructions (for developers only)

The project uses [Zig](https://ziglang.org/) as the build system. To build the
project, download the [Zig compiler version
0.15.2](https://ziglang.org/download/0.15.2/zig-x86_64-windows-0.15.2.zip),
extract it to a path of your liking and invoke `zig build`.

For a release build, use `zig build --release=fast`. To build directly to RBR
plugins directory, use `zig build --release=fast --prefix=/path/to/RBR/Plugins
--prefix-exe-dir . --prefix-lib-dir .`

The provided `Makefile` can also be used to execute these commands easier.

For `compile_commands.json` to be used with C++ language servers, invoke `zig
cdb`.

To build d3d9.dll, build
[dxvk-openRBRVR](https://github.com/Detegr/dxvk-openRBRVR) using meson. I used
`meson setup --backend=vs2022 --build_type=release` to configure it.

## Thanks

- [Kegetys](https://www.kegetys.fi/) for RBRVR, showing that this is possible.
- [TheIronWolf](https://github.com/TheIronWolfModding) for patching VR support
  for D3D9 into dxvk.
- Towerbrah for the idea to implement VR support using TheIronWolf's fork and
  the help debugging RBRHUD+RBRRX issues.
- [mika-n](https://github.com/mika-n) for open sourcing
  [NGPCarMenu](https://github.com/mika-n/NGPCarMenu) and collaborating to make
  RSF and RBRControls work with the plugin.

## License

Licensed under Mozilla Public License 2.0 (MPL-2.0). Source code for all
derived work must be disclosed.
