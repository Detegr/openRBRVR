# openRBRVR

![openRBRVR logo](img/openRBRVR.png)

Open source VR plugin for Richard Burns Rally.

## Features

- Performance overall seems better compared to RBRVR. On some stages the
  difference is huge, on others they're quite similar. I haven't yet found a
  stage that would run noticeably worse on openRBRVR.
- The image is more clear with openRBRVR with same resolution compared to
  RBRVR.
- Vulkan backend via dxvk ([fork](https://github.com/TheIronWolfModding/dxvk)
  by TheIronWolf that adds D3D9 VR support).
- PaceNote plugin UI works correctly.
- Gaugerplugin works correctly (but has a performance impact).

## Installation instructions

The installation instructions assume that openRBRVR is installed on top of the
[RallySimFans](https://rallysimfans.hu) plugin.

- Disable Virtual Reality from RSF launcher
- Copy d3d9.dll into RBR root folder
- Copy openRBRVR.dll and openRBRVR.ini into RBR/Plugins

If you have RBRVR installed:

- Rename openvr_api.dll to openvr_api.dll.rbrvr
- Copy openvr_api.dll into RBR root folder

To switch back to RBRVR:

- Delete d3d9.dll (from openRBRVR)
- Delete openvr_api.dll (from openRBRVR)
- Rename openvr_api.dll.rbrvr to openvr_api.dll
- Enable Virtual Reality from RSF launcher

Note: avoid launching the RSF launcher while openRBRVR is installed will cause
confusion for the launcher and it might overwrite files and other settings.
Always run RBR from `RichardBurnsRally_SSE.exe` directly.

## Setup

In `Plugins/openRBRVR.ini` you can change the following values:

- `superSampling` supersampling setting for the HMD. I'm running this at 2.0
  with no problems (SteamVR SS 100%). The default is 1.0
- `menuSize` the size of the rendered 2D plane when menus are visible. The
  default is 1.0
- `overlaySize` the size of the rendered 2D plane where the overlays are
  rendered while driving. The default is 1.0
- `overlayTranslateX` overlay (2D content) translation in the X direction
- `overlayTranslateY` overlay (2D content) translation in the Y direction
- `overlayTranslateZ` overlay (2D content) translation in the Z direction
- `lockToHorizon` counters the car motion for the headset's pose so that the
  driver does not move with the car but the car moves around the driver's head.
  Values 1, 2 and 3 are supported where 1 locks the roll axis, 2 locks the
  pitch axis and 3 locks both.
- `debug` enable to see debug information on the screen

With the desktop FoV, objects might disappear from your peripheral vision
before they go out of frame. On some stages this is more apparent than others.
This can be fixed either from the RSF launcher, NGPCarMenu, or runtime by using
the PaceNote plugin (double click right mouse button to open up the menu).
Values between 2.3-2.6 work fine for my wide FoV headset.

### Overriding FoV with the RSF launcher

The openRBRVR plugin is not supported by RSF. It will overwrite d3d9.dll (and a
couple of other files, if you accidentally enable Virtual Reality from the
launcher). To set the FoV override, do the following steps:

- In RSF launcher, set "Override FoV" to a larger value.
- Close RSF launcher.
- **Copy openRBRVR d3d9.dll into the RBR folder again**.
- Launch `RichardBurnsRally_SSE.exe`.

## Known bugs and limitations

- The seat position cannot be changed via keyboard. PaceNote plugin can and
  should be used instead. The UI works as expected.
- The VR view can only be recentered from Options -> Plugins -> openRBRVR ->
  Recenter view. There is no runtime keyboard capture implemented in the
  plugin. RBRControls plugin (included in RSF install) will have support for
  recentering the view from a wheel button in the upcoming release.

## Build instructions

The project uses CMake. I've been developing it with Visual Studio 2022
community edition that has CMake support built in.

To build d3d9.dll, build `dxvk` using meson. I used `meson setup
--backend=vs2022 --build_type=release` to configure it.

## Thanks

- [Kegetys](https://www.kegetys.fi/) for RBRVR, showing that this is possible.
- [TheIronWolf](https://github.com/TheIronWolfModding) for patching VR support
  for D3D9 into dxvk.
- Towerbrah for the idea to implement VR support using TheIronWolf's fork and
  the help debugging RBRHUD+RBRRX issues.
- [mika-n](https://github.com/mika-n) for open sourcing
  [NGPCarMenu](https://github.com/mika-n/NGPCarMenu).

## License

Licensed under Mozilla Public License 2.0 (MPL-2.0). Source code for all
derived work must be disclosed.
