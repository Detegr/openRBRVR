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

## Dealing with Z-fighting

Z-buffer near-far clipping range that's suitable for VR use is somewhere around
0.01 near - 10000 far. 0.01 (1cm) seems to be a fine enough range for near
value and some stages (like Sleeping Warrior 2019) render scenery FAR away, so
the large value needs to be somewhere around 10000. If those values are used,
the Z-buffer isn't precise enough even if the D32F precision is used due to the
large range between near and far planes.

To combat this, starting from version 0.3.0 openRBRVR renders the car geometry
with 0.01 near-10000 far range and the stage with 1.0 near-10000 far. The far
value has to be the same as for the other range, otherwise the skybox in some
stages does not render correctly. The exact reason why this happens is
currently unknown. When done this way both the car interior and the stage are
rendered with higher precision, reducing z-fighting considerably.
