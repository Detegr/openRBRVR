# Experimental OpenXR hands and direct-touch menu

This implements hands using Valve's textured red/black glove meshes and an
small hand menu with placeable Start and Call for help cubes. One setting enables
all hand features; it defaults off, cockpit driving only. OpenVR is
unaffected. Enable `[OpenXR] handTracking = true` with `runtime = 'openxr'`, or
use and save the OpenXR menu toggle.

## Small hand menu

Enable **Show hands** in the OpenXR settings and save,
or set the following in `Plugins/openRBRVR.toml`:

```toml
[OpenXR]
handTracking = true
```

This single setting enables tracked gloves, the palm-facing menu icon, local
pinch activation, and both placed buttons. Turning it off disables all of them
and releases ignition. Old `handMenu`, `handMenuGesture`, and `handMenuIcon`
keys are ignored and removed on the next config save. Per-car saved placements
are retained.

During focused cockpit **Driving** with positional head tracking, hold the left
palm toward the headset for 250 ms, within 18–75 cm. A small circle containing
three lines appears beside the palm, clear of extended fingers. The icon depends
on the palm pose, not on curling fingers or releasing a pinch. A tilted open
palm is accepted up to about 70 degrees from facing the headset. Release the
thumb/index pinch first, then pinch to toggle the menu. With the panel open, move either index fingertip
from in front of a button into **Place start button**, **Close**, or **Place call for help**. Hover turns a button
bright red; contact flashes the button and active fingertip indicator amber. Withdraw at least
35 mm in front of the panel before another touch can activate. Sliding between
buttons while touching cannot select another button.

The 18×17.4 cm panel captures its reference-space pose 21.4 cm above the palm when
opened, with its labels facing the headset. It stays there as the hand moves.
Both hands must be valid to interact. Temporary hand tracking loss retains the
open panel at its captured pose and keeps locked cubes visible. Touches, ignition
and help countdowns are cancelled immediately; interactions resume after tracking
returns and fingers withdraw. An unfinished placement is retained but hidden
until its right index is tracked again. The hand menu has no Recenter action.
When recentering through an existing binding or the regular plugin menu, the
runtime locates the old reference space in the new one before destruction and
that transform rebases the captured panel/cube, preserving physical placement.
Both touch states reset and require withdrawal before another activation; held
pinches require release as well. An unavailable/invalid reference transform
closes the panel safely. Close and a new left-hand gesture still dismiss it.

This version supports cockpit driving only. Pause, pre-stage, menus, replay,
external cameras, 3DoF, focus loss, session stop/recreation, hand disable, invalid
head poses and reference-space changes close and reset it. Hand loss and
reacquisition require a new observed release, so reacquiring a held pinch or
fingertip already inside a button cannot activate an action. Hands continue to
use the existing cockpit rendering path while operating the menu.

### Stage-start crash fix (2026-10-09)

The `KSON2_2026 10 09-21 02 45.dmp` crash had EIP=0, returning into
`rbr::digital_input+0x3b` from the NGP physics thread. The ignition detour had
been enabled by a temporary hook before its original-call trampoline was moved
into the global object read by that detour. A concurrent input poll could call
its still-null pointer. Install now writes the trampoline directly into that
object before enabling the patch. Failed activation removes the disabled hook
and clears its state. A deterministic mock enters the detour during enable to
exercise this timing and also checks create/enable failure cleanup. In-game
stage-start verification is still required after deployment.

### Place start button

Touch **Place start button** with either index to begin placement. A blue 5 cm
cube follows the right index fingertip. Hold that fingertip at the desired
physical location, then touch **Place start button** again with the **left**
index to lock it. Right-index presses cannot lock placement. Close or the menu
closing gesture cancels unfinished placement. To reposition a locked cube, use
Place start button again and repeat the same steps.

The locked cube and fingertip markers are visible only while the menu is open.
The cube remains touchable at its captured position when hidden; closing or
opening the menu does not interrupt held ignition. Withdraw your
fingertip at least 3.2 cm from the cube centre along any cube axis, then touch
its 2.5 cm half-width volume with either index. It highlights on approach and
stays amber while held down. Contact holds ignition down until the fingertip
withdraws beyond the 3.2 cm release region (7 mm beyond its surface). This hysteresis prevents flicker
near the cube surface. With both fingertips touching, the last finger leaving
releases the input. There is no timed tap, repeat timer or press cooldown.
Ignition stays up while placing, when locking with a fingertip inside,
or on tracking recovery with a fingertip already inside.

Contact supplies a held RBR **ignition** input level. The native input hook uses
ignition axis 12 (documented in
[NGPCarMenu's RBRAPI](https://github.com/mika-n/NGPCarMenu/blob/master/src/RBRAPI.h))
and verifies the installed SSE executable's ignition arguments/result handling
before hooking its live call target. Redirected calls are supported, preserving
the existing input function/plugin chain and physical bindings.
Unsupported executable layouts or hook failures disable the start action and
log the reason without preventing VR startup. Valid hand frames renew the held
state; withdrawal releases immediately on the next input poll. Focus,
tracking/eligibility loss, recenter and shutdown also release it. A 500 ms
frame-stall timeout prevents a stuck hold if VR updates stop arriving. The
actual RBR/NGP engine start still needs in-game hardware validation.

Locking either cube automatically saves its position and orientation in the
current car's `_personal.ini`, in `[openRBRVR]`. There is no extra Save step.
Reposition and lock again to overwrite that button's saved pose. Existing seat,
camera and other INI values are preserved. Save success/failure appears as a
short game message and in `Plugins/openRBRVR-hands.log`; a failed save leaves
the current in-memory placement usable.

The versioned keys `handStartButtonV1` and `handHelpButtonV1` contain seven
space-separated numbers: position XYZ in metres, then quaternion XYZW. Poses
use the game's recentered cockpit reference frame, rather than raw runtime room
coordinates. Recenter into your normal driving position before placing buttons.
On car/stage selection or VR session recreation, valid saved poses restore once
the cockpit seat is loaded and any configured startup recenter sequence is done.
With automatic recenter disabled, restoration uses the current cockpit reference;
use your usual seated calibration consistently between sessions. Runtime room
origin changes and motion-rig alignment still need headset testing.

Car identity resolves through `Cars/cars.ini` to the model's `_personal.ini`,
so layouts follow the car model, rather than an RSF-reused slot number. Returning
from loading/pre-stage re-resolves this path even for the same slot and stage.
Two entries referring to the same model INI share a layout. Missing, malformed,
non-finite or implausible poses leave just that button unplaced; bad files do not
block VR startup. Only a newly locked button is written; no per-frame disk writes.
To clear a saved placement, remove its key from the car's personal INI and reload
the car. Existing session-only placements must be locked again once to save them.

Recenter rebases live cubes even while the menu is closed, retaining their physical
positions without overwriting their saved cockpit poses. Temporary pause, camera/
mode changes or focus loss disable contact and require withdrawal on return.
Session/reference-space recreation clears live contacts and restores saved poses.
The same cockpit-driving-only eligibility and stereo rendering path apply.
Check `Plugins/openRBRVR-hands.log` for **ignition input hook ready**, placement
and lock messages, **ignition DOWN/UP** contact transitions, and **ignition
DOWN/UP sampled by RBR input poll**. The sampled messages establish that the
game observed the transitions; engine behavior still depends on normal game/NGP
conditions. Logs record transitions rather than every held frame.

Headset checks: lock with the right fingertip held still; confirm no immediate
start, then withdraw/touch and hold to start a stalled engine. Verify DOWN is
held and UP arrives on withdrawal; try either/both indexes, close the menu,
recenter with it closed, and test
pause/focus/tracking loss and recovery. Compare against your normal ignition
button, and check cube depth and position in both eyes/multiview/quad views.

### Call for help

Touch **Place call for help** to put a second cube on the right index. Move it
where you want, then touch **Place call for help** again with the left index to
lock it. Start and help placements are independent; only one unfinished
placement can follow the right finger at a time. Close cancels an unfinished
placement. Both locked cubes are visible only while the menu is open and work
while hidden. Both placements are automatically saved per car when locked.

Withdraw after locking, then touch the help cube with either index for **two
continuous seconds**. Its progress bar fills and it flashes amber when fired.
Lifting early cancels the countdown. Contact uses the same cube geometry and
release hysteresis as the start button. Fingers cannot add their durations
together. A held finger must withdraw before another countdown can start.
Closing or opening the menu does not cancel a locked cube's pending hold.

Invalid tracking, focus/mode loss, recenter or session reset cancels a pending
hold. Tracking recovery with a fingertip already inside requires withdrawal
first. A gap over 250 ms between tracked samples restarts the countdown.

The action uses the same game-window message as the installed RBRControls
CALLFORHELP binding: `WM_COPYDATA`, sender tag `0xDEAF01`, action ID `4` in
`COPYDATASTRUCT.dwData`, and no payload. This was traced in the installed RBRControls DLL;
its binding dispatches this message to the D3D device's focus window. No private
plugin function, keyboard mapping, forced teleport or alternate rescue logic is
used. The active game's message handler must support that RBRControls protocol;
other installations may not. Delivery/result diagnostics go to the hands log.
Actual rescue behavior and compatibility with the user's game conditions need
in-game validation. Trigger it while stuck and compare with the normal bound
Call for Help button. Test early release, a full two-second hold, continued
contact after activation, both fingers, tracking loss, and Close during a hold.

Headset persistence checks: place and lock both cubes in car A, restart VR/RBR,
and confirm they restore without activating. Switch to car B and place different
positions, then return to A. Repeat a selection using the same stage/car slot.
Check the save/load path in the hands log, seat position preservation, startup
recenter enabled/disabled, and recovery with a finger already inside a cube.

### Local palm gesture

The menu uses only the tracked-joint left palm-facing thumb/index pinch.
There is no separate gesture setting or in-game selector. Older gesture
configuration values are ignored and removed on the next save.
`XR_FB_hand_tracking_aim` is no longer requested or processed. Ordinary hand
tracking and optional unobstructed-hand data-source selection remain enabled.

Local palm detection uses the official
[OpenXR hand-joint convention diagram](https://raw.githubusercontent.com/KhronosGroup/OpenXR-Docs/main/specification/sources/images/ext_hand_tracking_joint_convention.png):
local **−Y** points out of the palm for both hands. It is rotated by the
already VIEW-to-rendering-reference converted joint orientation and compared
with the direction toward the compensated VIEW head position. Joint and head
poses share the same locate time and reference space. Local pinches use 22 mm
on / 38 mm off thresholds, 250 ms facing stability and a 550 ms cooldown.
All gesture, layout and touch tuning constants are together in
`src/HandMenu.hpp`.

The custom icon appears automatically while the palm is facing the headset.
The local detector cannot identify a reserved Quest system gesture or establish
system-icon visibility. It does not intercept or suppress Quest system UI.
Turn off **Show hands** if the gesture conflicts with system UI.

`Plugins/openRBRVR-hands.log` records the local gesture choice at initialization,
panel open/close/reset, tracking loss/recovery, placement save/load and actions.
Gesture events log on transitions, rather than every frame. Hardware behavior
of this simplified version still needs headset validation.

### Hand menu headset checklist

1. Check hand-tracking capability logs and the `local palm pinch only` message
   for the actual **32-bit PC runtime and streaming connection**.
2. With both hands valid, face the left palm toward the headset: verify the
   stable-facing icon, pinch opening, release/re-pinch closing and fixed panel
   placement.
3. Approach each button from the front with each index fingertip. Check
   hover, amber contact feedback, Close and start-button placement/left-only lock.
   The hand menu must contain no Recenter item. Test held ignition and release
   on withdrawal; neither locking nor tracking reacquisition inside may press.
4. Hold a pinch or contact through the cooldown. Verify one action only; slide
   across the buttons and jitter near their edges/depth. Withdraw to rearm.
5. Occlude each hand, reacquire while pinching/touching and verify no action
   until released/withdrawn. The panel and locked cubes remain visible through
   hand loss; an unfinished cube resumes following the right index on recovery.
   Invalid head tracking must close the panel.
6. Pause/resume, replay, external camera, 3DoF, hand disable, focus loss/return,
   session restart and runtime recenter: verify closure and fresh activation.
7. Check both eyes, stereo, DXVK multiview, quad views, MSAA and cockpit depth.
   With motion compensation, check ordinary leaning and rig movement; compare
   rendered button edges against the contact positions.
8. Observe whether Quest system UI conflicts. The local-only implementation
   cannot detect reserved runtime gestures. Turn off Show hands if the gesture
   causes conflicting actions.

`zig build test` covers optional data-source selection, local gesture stability,
pinch hysteresis, release/cooldown, tracking recovery, touch boundaries/depth,
sliding/rearm, fixed placement, local-coordinate hit testing, both actions and
menu reset. Additional tests cover tilted open-palm icon visibility, icon
visibility during a held pinch without activation, either fingertip, simultaneous
presses and reference-space rebasing after recenter. Start-button tests cover
right-index following, left-only lock, lock/contact release, cube boundaries,
hysteresis, held input, last-fingertip release, closed-menu use, tracking/mode
recovery, recenter rebasing, held input renewal/frame-stall release, and native
versus redirected ignition call targets. Help tests cover the two-second
boundary, early release, frame stalls, invalid samples, either finger, lack of
repeat while held, independent placement, left-only lock, tracking recovery,
hidden-cube use, Close preserving the hold, and reference-space rebasing.
Persistence tests cover pose round trips, separate car files, independent updates,
seat/camera/other INI preservation, missing/invalid files, write failures, and
fingertips already inside restored cubes. Configuration tests cover the single
hands setting and migration from obsolete menu/icon/gesture keys. These mock checks do not
establish headset behavior.

`zig build hand-preview --release=fast -- zig-out/hand-menu.bmp --menu` renders
the actual panel geometry, embedded labels and hover color on a hidden D3D9
device without RBR or a headset. This preview passed and was visually inspected
on 2026-10-09; it does not exercise in-game stereo, direct-touch comfort or a
real OpenXR runtime.

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
  `XR_EXT_hand_tracking_data_source` when exposed.
  They are enabled at instance
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
- With Show hands off, hand updates return immediately. Turning it off performs
  cleanup once: release ignition, destroy hand trackers, clear meshes, close the
  menu and cancel contacts/countdowns while preserving locked placements. Later
  disabled frames skip joint clearing, menu logic, eligibility/car lookup, pose
  calculations, file access and mesh generation. Normal VR frame/session handling
  and separately enabled motion compensation continue. The glove texture and mesh
  capacity are retained until VR shutdown for reuse; no frame-time improvement
  has been measured on a headset.
- Tracking data is cleared before each enabled frame; invalid head views also suppress
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
