## Unreleased

- Fix black screen in front of the windshield if Pacenote plugin is not active
- Fix car interior clipping too early when getting close to it
- Fix trackside object flickering (in Antaramanana II for example)

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
