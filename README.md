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

## Frequently asked questions

- See the [FAQ](https://github.com/Detegr/openRBRVR/blob/master/FAQ.md).

## Build instructions (for developers only)

The project uses [Zig](https://ziglang.org/) as the build system. To build the
project, download the [Zig compiler version
0.14.0](https://ziglang.org/download/0.14.0/zig-windows-x86_64-0.14.0.zip),
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
