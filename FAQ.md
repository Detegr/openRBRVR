# Frequently asked questions

[🇬🇧](FAQ.md) - [🇨🇿](FAQ_CZ.md) - [🇫🇷](FAQ_FR.md)

## The plugin does not start

### Direct3DCreateVR is missing

![Direct3DCreateVR missing](img/d3dcreatevr.png)

The `d3d9.dll` file is not from openRBRVR release. The RSF launcher sometimes
gets things a bit wrong, try the following steps to resolve the issue:

- Close RBR
- Change the VR plugin to RBRVR and back to openRBRVR (if you have both installed).

If this does not help, try the following steps:

- Close RBR
- Disable VR mode in RSFLauncher and make sure the 2D fullscreen mode is set to
  "Normal" and not as "Vulkan".
- Close RSFLauncher app and re-run the latest Rallysimfans\_Installer.exe app
  and choose "Update Existing Installation" (make sure the RBRVR and openRBRVR
  are ticked in the list of components).

## OpenComposite error: unsupported apptype

openRBRVR does not work with OpenComposite. It has a real OpenXR
implementation. Please disable OpenComposite when using openRBRVR.

### Game just crashes to desktop right away

- Make sure Light plugin is not enabled. This plugin is not compatible with it.
- If using openXR, please make sure your headset is supported.

## Can I use OpenXR instead of OpenVR/SteamVR?

OpenXR is supported for headsets that have 32-bit OpenXR runtime available.
Please refer to the following table to check if your device is supported:

| Manufacturer | Runtime                                                                    | Support     | Comments                                                                    |
| ------------ | -------------------------------------------------------------------------- | ----------- | --------------------------------------------------------------------------- |
| Pimax        | [Pimax-OpenXR](https://github.com/mbucchia/Pimax-OpenXR)                   | ✅          |                                                                             |
| Oculus       | Oculus OpenXR or [VDXR](https://github.com/mbucchia/VirtualDesktop-OpenXR) | ✅          |                                                                             |
| Pico         | [VDXR](https://github.com/mbucchia/VirtualDesktop-OpenXR)                  | ✅          |                                                                             |
| Reverb       | WMR with [OpenXR-Vk-D3D12](https://github.com/mbucchia/OpenXR-Vk-D3D12)    | ✅*         | Needs a synchronization workaround that has a potential performance impact  |
| Valve        | SteamVR OpenXR                                                             | ⛔          | No 32-bit runtime available                                                 |
| Varjo        | Varjo OpenXR                                                               | ⛔          | No 32-bit runtime available                                                 |

Enable OpenXR runtime from `Options -> Plugins -> openRBRVR -> VR runtime` or
edit `openRBRVR.toml` to contain `runtime = 'openxr'`. For Reverb headsets, select
`OpenXR (Reverb compatibility mode)` from the game or put `runtime =
'openxr-wmr'` in the toml.

## My FPS is worse than what it is in RBRVR

- Disable cubic env maps
- Make sure anti-aliasing is not too high. Generally 4x SRS is the highest you
  can go and most systems can't go over 2x SRS.
- If not needed, make sure you're not running the co-driver mode (also known as
  the bonnet camera desktop window mode). The setting can be found from
  `Options -> Plugins -> openRBRVR -> Desktop window settings -> Desktop window
  mode`. Use `VR view` or `Off` for better performance.

## How do I reset the VR view?

- Bind a button via `Options -> Plugins -> RBR Controls -> Page 2 -> Reset VR view`

## How do I move the seat?

- Enable 3-2-1 countdown pause from `Controls` page of RSF launcher to have
  time to adjust the seat before starting a stage.
- Open RBR and go to stage.
- Open PaceNote plugin by double clicking the RBR window with right mouse
  button. Adjust the seat, hit save and close the window from the `X` in
  top-right corner.

## How do I enable anisotropic filtering?

RSF launcher does not show an option to change anisotropic filtering. It can be
changed by modifying `dxvk.conf` by hand:

- `d3d9.samplerAnisotropy` Use one of 0, 2, 4, 8 or 16.

## The texts in RBR menus are missing

- Increase 2D resolution to 800x600 or above.

## The game is always shown on a "screen" within the VR environment

- This is normal for menus. If this happens also when driving, install SteamVR.

## What graphics settings should I use?

- Depends on the hardware. I use SteamVR at 100% scaling, anisotropic filtering
  at 16x with the following settings in the RSF launcher:

![Example settings](img/example_settings.png)

## There is shimmering and/or flickering of certain textures

- This can happen without Anti-Aliasing and/or anisotropic filtering. Increase
  anisotropic filtering and try increasing Anti-Aliasing if your system can
  handle it.
