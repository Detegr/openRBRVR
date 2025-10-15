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
| Reverb       | WMR                                                                        | ✅          |                                                                             |
| Valve        | SteamVR OpenXR                                                             | ⛔          | No 32-bit runtime available                                                 |
| Sony         | SteamVR OpenXR                                                             | ⛔          | No 32-bit runtime available                                                 |
| Varjo        | Varjo OpenXR                                                               | ⛔          | No 32-bit runtime available                                                 |

Enable OpenXR runtime from `Options -> Plugins -> openRBRVR -> VR runtime` or
edit `openRBRVR.toml` to contain `runtime = 'openxr'`.

## OpenComposite error: unsupported apptype

openRBRVR has a native OpenXR implementation, so OpenComposite is not required.
If you wish to run openRBRVR in OpenXR mode, try setting
`RichardBurnsRally_SSE.exe` to `SteamVR` mode in OpenComposite runtime
switcher, effectively disabling OpenComposite for RBR.

Alternatively, remove OpenComposite's global installation if you don't have use
for it.

## My FPS is worse than what it is in RBRVR

- Make sure anti-aliasing is not too high. Generally 4x SRS is the highest you
  can go and most systems can't go over 2x SRS.
- If not needed, make sure you're not running the co-driver mode (also known as
  the bonnet camera desktop window mode). The setting can be found from
  `Options -> Plugins -> openRBRVR -> Desktop window settings -> Desktop window
  mode`. Use `VR view` or `Off` for better performance.
- If using RBRHUD, there are some cars that use two RBRHUD config files. Those
  tend to run pretty poorly in VR. Renaming or deleting `config2.ini` from
  `Plugins/RBRHUD/Gauges/carname` can increase performance by 30% while
  removing very little immersion from the RBRHUD gauges.

## How do I reset the VR view?

- Bind a button via `Options -> Plugins -> RBR Controls -> Page 2 -> Reset VR view`

## How do I move the seat?

- Go to `Utils -> RBR Controls`
- Make sure JoyKey is ENABLED on the first page
- Go to page [4/4] openRBRVR and bind buttons for seat movement

## How do I enable anisotropic filtering?

RSF launcher does not show an option to change anisotropic filtering. It can be
changed by modifying `dxvk.conf` by hand:

- `d3d9.samplerAnisotropy` Use one of 0, 2, 4, 8 or 16.

## The texts in RBR menus are missing

- Increase 2D resolution to 800x600 or above.

## The game is always shown on a "screen" within the VR environment

- This is normal for menus. If this happens also when driving and you're using
  SteamVR mode, install SteamVR.

## What graphics settings should I use?

- Depends on the hardware. I use SteamVR at 100% scaling, anisotropic filtering
  at 16x with the following settings in the RSF launcher:

![Example settings](img/example_settings.png)

## There is shimmering and/or flickering of certain textures

- This can happen without Anti-Aliasing and/or anisotropic filtering. Increase
  anisotropic filtering and try increasing Anti-Aliasing if your system can
  handle it.

## How do I use per-stage anti-aliasing?

- Per-stage anti-aliasing is best used in a way that the base anti-aliasing is
  set to 2x SRS or 4x SRS in the RSF launcher, and the `[gfx]` sections reduce
  the default multisampling for example disabling for BTB stages it by setting
  it 0. See openRBRVR.toml.sample for example.

## Does openRBRVR support foveated rendering?

- Yes, in OpenXR mode for headsets that have [support for 32-bit
  OpenXR](https://github.com/Detegr/openRBRVR/blob/master/FAQ.md#can-i-use-openxr-instead-of-openvrsteamvr).
  You'll need to enable quad view rendering from `Options -> Plugins ->
  openRBRVR -> OpenXR settings -> Use quad-view rendering`
- After launching the game, if you take a look at the debug view
  of openRBRVR (`Options -> Plugins -> openRBRVR -> Debug settings -> Debug
  information`) it should show two different resolutions, for peripheral and
  focus views.
- The foveated rendering uses an open source OpenXR layer to do the heavy
  lifting. For additional configuration and fine tuning, refer to the
  [Quad-Views-Foveated layer
  documentation](https://github.com/mbucchia/Quad-Views-Foveated/wiki/Advanced-Configuration)
