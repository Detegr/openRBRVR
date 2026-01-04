## Setting up openRBRVR on Linux

These are the rough steps needed to run openRBRVR on Linux via OpenXR runtime.
Some people have been able to get ALVR to work with SteamVR but that's not the
focus of this writeup.

## Prerequisites

A setup I have been able to get working is:

- [openRBRVR 2.2.2](https://github.com/Detegr/openRBRVR/releases/tag/2.2.2)
- [My fork](https://github.com/Detegr/proton-ge-custom/releases/tag/GE-Proton10-woxr32-4) of Proton GE which supports and includes 32-bit version of wineopenxr
- [WiVRn](https://github.com/WiVRn/WiVRn) for streaming to HMD. I'm using Pico 4 but Quests should work as well. Running raw [monado](https://gitlab.freedesktop.org/monado/monado) probably works as well if you have a headset that is not a wireless one. You **have to have 32-bit version of WiVRn/monado runtime** in order for this to work. I'm afraid the WiVRn flatpak doesn't include that. I built one myself for testing.

There needs to be a json file describing the active 32-bit runtime. Mine points into `/nix/store` as I'm running NixOS, but yours should point to wherever you built the 32-bit version of WiVRn/Monado.

`~/.config/openxr/1/active_runtime.i686.json`:
```json
{
    "file_format_version": "1.0.0",
    "runtime": {
        "name": "Monado",
        "library_path": "/nix/store/gyd57v1fhskwvyxdzy0s75jlrfqi7gb5-wivrn-lib-32-25.12/lib/libopenxr_wivrn.so",
        "MND_libmonado_path": "/nix/store/gyd57v1fhskwvyxdzy0s75jlrfqi7gb5-wivrn-lib-32-25.12/lib/libmonado_wivrn.so"
    }
}
```

And again, make sure those are 32-bit binaries:

```bash
file /nix/store/gyd57v1fhskwvyxdzy0s75jlrfqi7gb5-wivrn-lib-32-25.12/lib/libopenxr_wivrn.so
/nix/store/gyd57v1fhskwvyxdzy0s75jlrfqi7gb5-wivrn-lib-32-25.12/lib/libopenxr_wivrn.so: ELF 32-bit LSB shared object, Intel 80386, version 1 (SYSV), dynamically linked, not stripped

file /nix/store/gyd57v1fhskwvyxdzy0s75jlrfqi7gb5-wivrn-lib-32-25.12/lib/libmonado_wivrn.so
/nix/store/gyd57v1fhskwvyxdzy0s75jlrfqi7gb5-wivrn-lib-32-25.12/lib/libmonado_wivrn.so: ELF 32-bit LSB shared object, Intel 80386, version 1 (SYSV), dynamically linked, not stripped
```

There are some issues with OpenXR layer loading (for foveated rendering and OBS mirror support) and multiview rendering at the moment. To circumvent those, use the following in `RBR/Plugins/openRBRVR.toml`:

```toml
[OpenXR]
quadViewRendering=false
xrApiPathModification=false
xrApiLayerObsmirror=false

[experimental]
disableMultiView=true
```

Make sure the runtime is set to `openxr` in either openRBRVR.toml or the RSF launcher.

Then, launch your game with the following environment variable set: `PRESSURE_VESSEL_IMPORT_OPENXR_1_RUNTIMES=1`. I used `umu-launcher` for this, pointing `PROTONPATH` to the custom version of Proton linked earlier in this document.
