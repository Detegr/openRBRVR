# Experimental OpenXR hand visuals

This implements visual-only hands using Valve's textured red/black glove meshes,
no gestures or gameplay input, default off, cockpit driving only. OpenVR is
unaffected. Enable `[OpenXR] handTracking = true` with `runtime = 'openxr'`, or
use and save the OpenXR menu toggle.

## Runtime support

Both extension enumeration and `XrSystemHandTrackingPropertiesEXT` must report
support. Tracker creation and joint-location errors are optional failures:
hands hide while the rest of VR continues. A creation failure is retried only
after toggling the feature off/on or restarting the OpenXR session. Location
failures recover on the next successful frame; repeated errors do not log every
frame.

Currently only the Quest 3 + Virtual Desktop / VDXR combination has been confirmed 
to provide hand tracking.

Should work with any 32-bit OpenXR runtime that exposes `XR_EXT_hand_tracking` and
supplies hand-joint data. The plugin does not generate fallback poses.

**Other runtimes may support hand tracking. Most likely:
Meta Quest 2 / 3S / Pro, PICO 4 / 4 Ultra.**

For Virtual Desktop, enable **Settings -> Streaming -> Advanced Options ->
Forward tracking data to PC** in the Quest app. The [VDXR developer describes
this opt-in requirement](https://community.khronos.org/t/handjointlocations-all-wrong/111702/7).

The application uses the core [Khronos hand-tracking API](https://registry.khronos.org/OpenXR/specs/1.1/man/html/xrLocateHandJointsEXT.html).
When available, [hand-tracking data-source selection](https://registry.khronos.org/OpenXR/specs/1.1/man/html/XrHandTrackingDataSourceInfoEXT.html)
requests unobstructed hands and rejects controller-derived skeletons. With only
the core extension, the runtime determines the source; that API alone cannot
distinguish controller emulation from bare-hand joints. The plugin does not
generate fallback poses.

## Lifecycle and rendering

- Optional instance extensions: `XR_EXT_hand_tracking`, plus
  `XR_EXT_hand_tracking_data_source` when exposed. They are enabled at instance
  creation to allow live toggling. If instance creation fails, it is retried
  without these optional extensions.
- Required entry points: `xrCreateHandTrackerEXT`, `xrDestroyHandTrackerEXT`,
  `xrLocateHandJointsEXT`, plus core `xrCreateReferenceSpace`, `xrDestroySpace`
  and `xrLocateSpace`, loaded through `xrGetInstanceProcAddr` so API-layer hooks
  are respected.
- Capability query: `XrSystemProperties` chained to
  `XrSystemHandTrackingPropertiesEXT`.
- Per hand: `XrHandTrackerEXT`, `XrHandTrackerCreateInfoEXT` with
  `XR_HAND_JOINT_SET_DEFAULT_EXT`, and 26 `XrHandJointLocationEXT` records.
- A dedicated identity VIEW space is created lazily with the hand trackers.
  It is independent of seat/recenter offsets and destroyed with the trackers,
  including partial creation failures. VIEW-space creation failure follows the
  same retry-on-toggle policy as tracker creation failure.
- Per frame: `XrHandJointsLocateInfoEXT` uses that VIEW space. `xrLocateSpace`
  locates VIEW in the current, possibly recentered LOCAL rendering space at the
  same prediction-dampened display time as the views and both hands.
  The layer-provided VIEW pose transforms each valid joint's position and
  orientation into rendering space. Radii remain unchanged.
  `XrHandJointLocationsEXT::isActive`, valid joint flags and finite data gate
  visibility. Optional data-source state also gates bare-hand visibility.
- Tracking data is cleared before each frame; invalid head views also suppress
  hands. Trackers are destroyed on disable and before session destruction,
  including automatic session restarts. Partial creation is cleaned up.
- Mesh vertices remain in OpenXR reference-space metres. Eye pose and projection
  are applied once, with no extra game-camera or horizon-lock transform.
  The same geometry feeds stereo, quad views and DXVK fixed-function multiview.
- The draw occurs after the game's eye scene, before its render target is
  finished. It keeps cockpit depth, uses the actual reversed-Z comparison,
  and restores D3D9 state with an all-state block. Draw failure is nonfatal.
- Valve's left/right glove assets are embedded in the DLL, with bone names mapped
  to the 26 OpenXR joints. CPU linear blend skinning uses absolute joint poses
  multiplied by the corresponding inverse bind matrices. Smooth vertex normals
  provide simple directional shading. No runtime asset importer is required.
- One shared 512x512 BC1 red/black texture has ten offline-generated mip levels.
  A managed D3D9 texture is created lazily, survives device resets and is released
  on shutdown. Texture creation/upload failure hides the visuals and is retried
  at most once every five seconds. There are no persistent hand vertex/index
  buffers, action bindings, controller input changes, wheel anchors or IK.

The implementation is split into `HandTracking` (capability/lifecycle/joints),
`HandMesh` (CPU skinning), `HandAssets` (embedded geometry/texture), `HandTexture`
(pitched mip uploads) and the OpenXR frame/draw integration. See the
[asset source, license and conversion notes](assets/valve_hands/README.md).

## Automatic motion compensation

This needs no additional user setting. The existing tracked-hands setting still
defaults off. The conversion is applied whenever hand visuals are enabled,
whether or not a motion rig or compensation layer is installed, and is not
gated by the plugin's controller-based `motionCompensation` option. An implicit
API layer can be active independently of that option.

For raw head pose `H`, hand joint pose `J`, and the layer's correction `C`, the
runtime returns head-relative joints `inverse(H) * J`, while the layer returns
the corrected head pose `C * H`. Their product is:

```text
(C * H) * (inverse(H) * J) = C * J
```

This follows translation and rotation without reading rig data or duplicating
the layer's filters. When compensation is inactive, `C` is identity and the
original joint poses are reconstructed. Actual head leaning/turning with hands
fixed on the wheel does not move the hands in cockpit space; actual hand
movement remains visible. No correction is cached across frames or layer toggles.

If the head-space query fails, its position/orientation flags are invalid, or
its pose is nonfinite/degenerate, both hands hide for that frame and recover on
the next valid query. The menu reports `invalid head pose`; diagnostics record
the head query result and flags. There is no raw-LOCAL fallback that could mix
compensated views with uncompensated hands.

The existing OpenXR Motion Compensation `compensate_controllers` setting handles
action spaces through `xrLocateSpace`, not `XR_EXT_hand_tracking` joints. This
VIEW conversion supplies compatibility within openRBRVR without modifying the
layer. See the [checked upstream implementation](https://github.com/BuzzteeBear/OpenXR-MotionCompensation/blob/1416c2ea192fcba6706f61ef2ac32d3c2265c3db/XR_APILAYER_NOVENDOR_motion_compensation/layer.cpp#L935-L1004).

## Validation

### In-game diagnostics

The OpenXR menu's `Hand input` line shows the PC runtime's live left/right
status, including in menus where hand drawing is intentionally hidden. The
plugin writes `Plugins/openRBRVR-hands.log` anew on each launch. While hand
visuals are enabled, it logs joint status and cockpit/frame eligibility every
five seconds, and successful draw status or errors at the same limited rate.
Tracker creation/capability errors are also recorded. This distinguishes no
PC hand data (`inactive`) from invalid joints and graphics failures. No joint
coordinates are written.

The hand renderer supports an existing D3D9 scene: if DXVK reports that RBR has
already begun one, it borrows that scene and leaves it open. A scene opened by
the hand renderer is closed by the renderer. This ownership behavior is covered
by the mock tests.

### Automated and headset checks

Run `zig build test` for the 32-bit mock-runtime tests. These cover absent
extensions, unsupported systems, property-query/function-loading failures,
partial tracker creation and cleanup, retry on toggle, shutdown, independent
hand loss/reacquisition, locate failures, invalid/nonfinite joints, controller
source rejection, recentered space/time propagation, bind-pose reconstruction
for both gloves, rigid pose transforms, local finger deformation, index offsets,
texture mip layout, pitched uploads and lock/unlock failure handling, and
configuration save/load/copy/default behavior. No headset is needed.

Motion-compensation tests also cover ordinary head translation/rotation with
stationary hands, actual hand movement, rig surge/heave/pitch/yaw with and
without compensation, live compensation changes, arbitrary partial corrections,
recentered reference spaces, equal head/joint timestamps, identity VIEW-space
creation/cleanup, and head-query failures, invalid flags and nonfinite poses.

`zig build hand-preview --release=fast` also checks texture creation/upload and
indexed mesh drawing on a real 32-bit D3D9 device without RBR or a headset. It
saves `zig-out/valve-gloves.bmp` with both hands open, palm-facing and curled.
On 2026-10-08, this preview passed on both native D3D9 and the installed
`2.6-openRBRVR` DXVK DLL; the rendered images matched pixel for pixel.

Run `zig build --release=fast` for the RBR plugin. Output is
`zig-out/bin/openRBRVR.dll`; runtime dependencies are unchanged. Redistribute
the generated `zig-out/bin/Valve-hand-models-LICENSE.txt` with the binary.

The following still require RBR and a connected headset:

1. Enable hands and verify both palms and all fingers in a cockpit driving view.
2. Occlude each hand, then reacquire it. Verify there are no frozen/fallback hands.
3. Recenter, move the seat, change world scale, and enable horizon lock. Check
   that tracked hands follow the physical hands without doubled head motion.
4. Pause, enter menus, switch to external cameras and play a replay. Hands should
   hide in all of these contexts; returning to cockpit driving should recover.
5. Compare depth against the wheel/dashboard; check both eyes, MSAA, stereo,
   quad views, multiview and BTB stages. Verify later game/overlay draws retain
   their normal graphics state.
6. Toggle hands, restart the OpenXR session and exit/relaunch. Check stability,
   tracker cleanup and frame time with the feature on versus off.
7. Run on an actual runtime without hand tracking, with the setting both on and
   off, to confirm startup and ordinary rendering continue normally.
8. With the rig stationary, lean/turn your head with hands held on the wheel.
   Confirm the gloves stay on the wheel, then verify actual hand movement.
   Activate motion compensation and drive through braking/surge and pitch/roll;
   check that rig motion is removed from the gloves along with the view. Toggle
   compensation, recenter and briefly lose hand tracking to check recovery.

The prior procedural renderer was confirmed visible in the headset. The Valve
asset checks establish mesh deformation and D3D9 rendering outside RBR, but
glove fit, in-game stereo/depth and performance still need headset validation.
Automatic hand motion compensation has passed simulated pose/lifecycle tests;
its interaction with VDXR and the installed layer still needs a motion-rig run.
