## 2.0.2

- Add support for OpenXR-Layer-OBSMirror (https://github.com/Jabbah/OpenXR-Layer-OBSMirror)

## 2.0.1

- Fix a bug that caused main menu to render in a funny way when the VR desktop
  view was set to bonnet cam
- Add configuration parameters to adjust whether the VR view should
  automatically recenter at the start of a VR session or at the start of each
  stage

## 2.0.0

- Implement multiview rendering. Reduces CPU load significantly and fixes some
  rendering issues like some shadows on BTB stages. Increases the GPU load a
  little.
- Use 3D scene as background when the main menu is active
- Make debug button toggle between debug modes
- Fix a rendering bug where the skybox was rendered in different colors for
  different eyes
- Fix a small logic issue when parsing openRBRVR.toml

## 1.1.1

- Comment out per-stage settings in openRBRVR.toml

## 1.1.0

- Update DXVK version to 2.4.1
- Use Pimax's native quad view rendering over Quad-Views-Foveated if it's
  enabled
- Add 32-bit OpenXR runtime validation in initialization
- Improve OpenXR initialization error messages
- Fix invalid openRBRVR version number shown inside the game
- Fix "Could not create VR render target for view 0" by changing the way
  available depth surface formats are tested

## 1.0.4

- Fix a typo in predictionDampening setting

## 1.0.3

- Fix white flashes that happened sporadically in OpenXR mode if anti-aliasing
  was not enabled
- Fix a rendering bug in co-driver mode
- Fix Rally School mode not loading the saved seat position correctly

## 1.0.2

- Implement prediction dampening for OpenXR

## 1.0.1

- Fix DirectX state block usage with reverse-z buffer to fix a bug where the
  car was invisible on some BTB stages
- Improve error messages in OpenXR initialization

## 1.0.0

- Use reversed Z-buffer in driving mode and replays. This eliminates all track
  side object Z-fighting issues (flickering signs etc.) from the game
- Fix a crash caused by reloading the OpenXR session when toggling quad view
  rendering on and off
- Improve performance of windscreen particle effects only mode

## 0.9.2

- Fix a rendering issue with foveated rendering where the peripheral view had
  white flashes if the frame rate dropped
- Fix a bug where quad view rendering was enabled by default
- Ship a build of Quad-Views-Foveated with the plugin and load it explicitly at
  startup if needed. Manual installation of the OpenXR layer is no longer
  needed to use foveated rendering.
- Tweak stage Z-near value to fix the stage clipping in tight places
- Fix rendering of windscreen effects (water/snow). By enabling particles but
  disabling particle rendering in openRBRVR settings, it is possible to have
  the water/snow effects on the windscreen but not have other particles visible
  for better performance.

## 0.9.1

- Fix a bug where a BTB stage was not detected correctly for disabling quad
  view rendering.

## 0.9.0

- Change the OpenXR backend to use D3D11 mode. This mode is more compatible
  with third party OpenXR layers.
- Implement support for XR_VARJO_quad_views. This in conjunction of
  [Quad-Views-Foveated](https://github.com/mbucchia/Quad-Views-Foveated) OpenXR
  layer enables foveated rendering for openRBRVR, resulting in a lot of
  performance improvement on original format stages. Sadly, BTB stage rendering
  is CPU bound and the performance there is worse with quad view rendering so
  openRBRVR automatically disables the feature for BTB stages if it's in use.
- The change of the OpenXR backend also enables support for
  [OpenKneeboard](https://openkneeboard.com/). The version that will work with
  openRBRVR is not yet released, the support should land in OpenKneeboard
  1.9.0. OpenKneeboard can then be used to bring desktop windows into the VR
  environment, like SimHub overlays.
- Implement support for [OpenXR motion compensation
  layer](https://github.com/BuzzteeBear/OpenXR-MotionCompensation) (OXRMC),
  contributed by MatteKarla. The support for openRBRVR is already out in OXRMC
  version 0.3.6 onwards.
- Improve OpenXR layer detection and error handling at startup.

## 0.8.2

- Fix a crash that occurred if the car's ini file had invalid data

## 0.8.1

- Fix a bug where the seat position was not loaded correctly if a stage was
  restarted

## 0.8.0

- Fix desktop window and 2D content window size calculation
- Implement seat movement via RBRControls
- Add explanation and troubleshooting tips for OpenXR error -32
- Smooth out the camera movement for the feature that rotates the yaw of the
  horizon locked camera 180 degrees if the car is upside down.
- Disable the aforementioned feature by default.
- Make the aforementioned feature toggleable on/off.
- Implement per-stage anti-aliasing.

## 0.7.5

- Fix a bug where the plugin would not launch if openRBRVR.ini was missing
- Add a note for installing OpenXR-Vk-D3D12 if OpenXR initialization failed
  because it was missing

## 0.7.4

- Allow horizon locking in replays
- To avoid horizon locked camera pointing backwards, the camera now turns 180
  degrees if the car's pitch increases/decreases 90 degrees or more.
- Report more accurate FPS in the FPS debug mode

## 0.7.3

- Add "World scale" setting for OpenXR
- Fix a rare rendering issue where certain effects were rendered differently in
  different eyes. For example Mitterbach Tarmac at night.
- Fix a crash that occurred when running openRBRVR without a VR headset
- Add a less intrusive debug mode where just a colored FPS counter is shown
  instead of the full debug information

## 0.7.2

- Fix a crash when using SteamVR with Reshade
- Improve OpenXR recentering logic
- Fix strange floating shadows on BTB stages

## 0.7.1

- Fix a bug where the co-driver mode did not work if "Render pre-stage in 3D"
  option was active.
- Fix Call For Help camera being a wrong one in the co-driver mode.
- Fix replays being always stuck to co-driver camera if the co-driver mode was
  active.

## 0.7.0

- Render the desktop window with the desktop window's aspect ratio.
- Add adjustment menu and settings for the desktop window viewport.
- Fix incorrect rendering of broken windshield texture.
- Add a way to render the desktop view using 2D camera. This can be for example
  used to set a seat position for a co-driver to play using the desktop window
  while the driver plays in VR.

## 0.6.0

- Implement per-stage supersampling
- Change config file format from ini to toml
- Add overlay adjustment menu. Show overlay borders while driving when debug
  mode is active for easier adjustment.
- Attempt to fix intermittent recentering issues happening with Reverb in
  OpenXR mode.

## 0.5.2

- Do not crash the game if a VR headset is not connected
- Fix a bug in horizon lock amount loading and adjustment

## 0.5.1

- SteamVR: Improve concurrent Vulkan queue usage
- Make the amount of correction that lock to horizon applies adjustable

## 0.5.0

- Implement OpenXR support
- Change internal GPU synchronization logic

## 0.4.5

- Fix tracking lag that occurred for some Oculus users.
- Fix particle rendering issue where too much of the particles were visible
  inside the car cockpit.

## 0.4.4

- Changes to potentially improve the stability of the plugin.
- Fix black opaque square being rendered in front of the car in external
  cameras if "render car shadows" was active.

## 0.4.3

- Combine previous cockpit rendering detection logics. They both have issues
  with certain cars so both are now used to minimize car rendering issues.
- Fix a logic bug with "Always call Present" debug option that rendered it useless.

## 0.4.2

- Change the logic how cockpit rendering is detected. Fixes a rendering issue
  with Skoda Fabia R5.
- Change "Show desktop window" logic in a way that the desktop window rendering
  is disabled only when driving mode is active.

## 0.4.1

- Use correct aspect ratio from desktop window size for 2D planes.
- Add "Render replays in 3D" option.
- Add "Always call Present" debug option to the debug menu.

## 0.4.0

- Add support for anti-aliasing and anisotropic filtering
- Tweak stage rendering to reduce object clipping on close contact
- Improve internal error handling
- Renew the Options -> Plugins -> openRBRVR menu

## 0.3.1

- Fix main menu clipping too early
- Fix desktop window color rendering

## 0.3.0

- Fix black screen in front of the windshield if Pacenote plugin is not active
- Fix car interior clipping too early when getting close to it
- Fix trackside object flickering (in Antaramanana for example)
- Pause menu is now rendered over 3D content by default. 3D rendering can also
  be toggled on for the spinning camera and main menu. Thanks for @dancek for
  the contribution.

## 0.2.0

- Loading screen can now be toggled off
- `openRBRVR.ini` can now be written from the in-game options menu
- Overlay (digidash etc.) is now fixed to the car's rotation also in
  lock-to-horizon mode

## beta9

- Fix RBRHUD + BTB stage rendering issue
- Internal improvements

## beta8

- Fix crashes when loading certain BTB stages

## beta7

- Fix skybox and fog rendering

## beta6

- Add drawDesktopWindow option to openRBRVR.ini. Drawing of the desktop window can be disabled with this. The option is also available from Options -> Plugins -> openRBRVR
- Make overlay (the 2D content while driving) position fully adjustable. openRBRVR.ini keys overlayTranslateX, overlayTranslateY and overlayTranslateZ can be used to translate the overlay.

## beta5

- Fix rendering issues on Ahvenus I, Ahvenus II and Pirttikulma
- Fix an issue where the stuttering remained on stage once it had started

## beta4

- Render view from the left eye is on the desktop window
- Add better frame time debug information when debug mode is active
- Support lock to horizon for roll and pitch axes or both

## beta3

- Add support for displaying debug information
- Create openRBRVR.ini if it's missing
- Handle ASLR correctly

## beta2

- Fix an issue where the license text page crashed the game

## beta1

- First public beta of openRBRVR
