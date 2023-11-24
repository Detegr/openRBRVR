# openRBRVR

![openRBRVR logo](img/openRBRVR.png)

Open source VR plugin for Richard Burns Rally.

## Features

- Performance overall seems better compared to RBRVR. On some stages the
  difference is huge, on others they're quite similar. I haven't yet found a
  stage that would run noticeably worse on openRBRVR, apart of a couple of
  buggy BTB stages. I'm also running openRBRVR with 200% supersampling where in
  RBRVR I used 150% because 200% was not running smoothly anymore.
- Vulkan backend via dxvk ([fork](https://github.com/TheIronWolfModding/dxvk)
  by TheIronWolf that adds D3D9 VR support).
- PaceNote plugin UI works correctly.

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

- `menuSize` the size of the rendered 2D plane when menus are visible. The
  default is 1.0
- `overlaySize` the size of the rendered 2D plane where the overlays are
  rendered while driving. The default is 1.0
- `superSampling` supersampling setting for the HMD. I'm running this at 2.0
  with no problems (SteamVR SS 100%). The default is 1.0

With the desktop FoV, objects might disappear from your peripheral vision
before they go out of frame. On some stages this is more apparent than others.
This can be fixed either from the RSF launcher, NGPCarMenu, or runtime by using
the PaceNote plugin (double click right mouse button to open up the menu).
Values between 2.4-2.6 work fine for my wide FoV headset.

### Overriding FoV with the RSF launcher

The openRBRVR plugin is not supported by RSF. It will overwrite d3d9.dll (and a
couple of other files, if you accidentally enable Virtual Reality from the
launcher). To set the FoV override, do the following steps:

- In RSF launcher, set "Override FoV" to a larger value.
- Close RSF launcher.
- **Copy openRBRVR d3d9.dll into the RBR folder again**.
- Launch `RichardBurnsRally_SSE.exe`.

## Known bugs and limitations

- BTB stages don't work with RBRHUD. For some reason the track geometry
  disappears when RBRHUD is enabled.
- Some BTB stages have rendering artifacts. See *Buggy stages* section for more
  information.
- PaceNote plugin causes rendering artifacts in BTB stages when the PaceNote
  plugin menu is open. The plugin itself works correctly and the settings can
  be changed. When the plugin menus are closed, everything is back to normal.
- The seat position cannot be changed via keyboard. PaceNote plugin can and
  should be used instead. The UI works as expected (except on BTB stages it
  causes rendering artifacts).
- The VR view can only be recentered from Options -> Plugins -> openRBRVR ->
  Reset view. There is no runtime keyboard capture implemented in the plugin.
- The skybox fades to black in the opposing direction where the car is pointing
  at. Luckily you rarely look over your shoulder while driving :)
- Some occasional crashes might happen.

### Buggy stages

These stages are found with my limited testing to not be rendering correctly:

- Ahvenus I
- Ahvenus II

## Implementation

The main idea of the implementation is to:

- Render the 3D scene for both eyes
- Render the 2D content on top of the 3D scene for both eyes
- Send both textures to the VR goggles

On a more detailed level, the following is done:

- The projection and eye position matrices are obtained from the VR runtime.
- The `SetVertexShaderConstantF` DirectX call is rerouted to custom code. This
  sets up the matrices for the vertex shader.
- The MVP matrix of the 3D scene is changed to one that works with VR
  rendering. The original matrix is cancelled out and the VR projection is used
  instead.
- The `SetTransform` DirectX call is rerouted to custom code. The 2D content in
  RBR is not rendered with a shader but with a fixed function pipeline instead,
  so the matrices are patched in this function.
- RBR's 3D scene drawing function is rerouted to custom code. It is called once
  for both eyes. When leaving the draw function, a render target is set where
  the 2D content (menus and stuff) will be rendered.
- The `Present` DirectX call is rerouted to custom code. In this function the
  frame is ready and it is sent to the VR goggles.

Dealing with menus in a VR environment is a bit funky, so in order to prevent
issues with focusing to text and dealing with moving backgrounds etc. the
content is rendered on a 2D plane in a 3D space. This is quite usual solution
for menus in different VR games.

## Build instructions

The project uses CMake. I've been developing it with Visual Studio 2022
community edition that has CMake support built in.

To build d3d9.dll, build `dxvk` using meson. I used `meson setup
--backend=vs2022 --build_type=release` to configure it.

## Thanks

- [Kegetys](https://www.kegetys.fi/) for RBRVR, showing that this is possible.
- [TheIronWolf](https://github.com/TheIronWolfModding) for patching VR support for D3D9 into dxvk.
- Towerbrah for the idea to implement VR support using TheIronWolf's fork.
- [mika-n](https://github.com/mika-n) for open sourcing [NGPCarMenu](https://github.com/mika-n/NGPCarMenu).

## License

Licensed under Mozilla Public License 2.0 (MPL-2.0). Source code for all
derived work must be disclosed.
