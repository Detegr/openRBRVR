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

