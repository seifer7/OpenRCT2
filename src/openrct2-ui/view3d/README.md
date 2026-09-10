# Temporary 3D view

Keep fork-specific 3D behavior here, not in upstream files. This is an integrated module, not a loadable plugin; a small upstream patch remains necessary.

## Boundaries

- Controller.*: UI-owned session lifetime, SDL timing/key mapping, visible-cursor right-drag camera input, events, restoration, error handling, and frame orchestration.
- Camera.h, Geometry.h: independent camera math and reference clipping/projection helpers.
- Renderer.*: shared box/quad submission and the software reference renderer used by existing tests. The live view does not use its CPU pixel/depth buffers.
- OpenGLRenderer.*: live triangle submission, vertex buffers, shaders, GPU clipping/rasterization, and depth testing.
- MapAdapter.*: live map/tile access, initial camera framing, and terrain/water/placeholder geometry submission.
- TerrainGeometry.h: upstream slope flags and corner heights translated into world-coordinate vertices.
- Presentation.*: GPU presentation delegation and the controls label; no dynamic sprite tiles or CPU image uploads.

The controller supplies the camera; the map adapter submits geometry; the OpenGL engine delegates the 3D pass into its palette framebuffer before drawing the controls label. Keep compatibility with upstream map/drawing API changes in the adapters rather than spreading it throughout the renderer.

World X/Y/Z coordinates, terrain center-fan triangulation, both valley diagonals, steep slopes, placeholder rules, and camera movement are preserved by this separation.

## Remaining upstream hooks

- openrct2-ui/UiContext.cpp: controller ownership, event/drawing/cleanup delegation, and the GetController host bridge.
- openrct2-ui/windows/TopToolbar.cpp: menu entry and View3D::Enter().
- openrct2-ui/input/InputManager.cpp/.h: input-block guard and generic reset of private input state.
- openrct2-ui/input/MouseInput.cpp: discard normal mouse input while blocked, including queued events immediately after activation.
- openrct2-ui/drawing/engines/opengl/OpenGLDrawingEngine.cpp: bind the final framebuffer and delegate the 3D pass before flushing the overlay.
- openrct2/ui/UiContext.h: generic default no-op DrawSceneOverride and CloseSceneOverride hooks.
- openrct2/paint/Painter.cpp: bypass original scene drawing when the override draws.
- openrct2/Context.cpp: close the override before global image disposal.
- UI/test vcxproj files and test CMakeLists.txt: one module-local import/include each.

openrct2-ui/UiContext.h needs no 3D declarations. Core code never includes module headers. Keep new 3D logic out of these upstream hook locations. Private input queues remain owned by the input managers.

## Lifetime and input invariants

- The controller belongs to the UI context, not a global singleton. Its private session owns camera, renderer, and presentation resources.
- Release the session before the OpenGL context, global image storage, or SDL windows are destroyed. Scene cleanup and window cleanup are idempotent.
- Block normal shortcuts, tools, and scrolling while active. After exit, continue blocking until keyboard keys and mouse buttons are released.
- Keep the cursor visible with relative mouse mode disabled. Rotate the camera only for mouse motion with the right button held; ordinary motion and left-dragging must not rotate it.
- Esc and focus loss restore relative mouse mode, cursor visibility, and position. Quit/window events still reach the host.
- Drawing failures release resources, restore input, report the error, and fall back to the normal scene.
- Presentation does not change graphics settings or replace the game window.
- Entry requires the OpenGL drawing engine and a current context. Other backends and builds without OpenGL show an explanatory message without changing input state.

## Build registration and tests

Add UI sources to this directory and View3D.props, not the upstream project. CMake already discovers UI sources recursively; reconfigure after adding/moving files.

Tests and their View3D.props/sources.cmake manifests live in test/tests/view3d/. Camera/rasterizer tests need neither SDL nor the UI library. Terrain tests compare against core map heights. Filters: Map3DGeometry.*, View3DCamera.*, View3DRenderer.*.

After upstream syncing:

1. Review the small integration patch above and adapt changed APIs inside the adapters/controller.
2. Build and run the feature tests plus TileElementsViewTests.*.
3. Check terrain, water, and placeholders with OpenGL. Check that other display backends reject entry without affecting the normal view.
4. Check WASD, visible cursor, right-drag look, Shift, Esc, focus loss, and exit while movement keys are held. Verify ordinary mouse movement and left-dragging do not rotate the camera.
5. Check resizing, repeated entry/exit, restored game tools/shortcuts, and quitting while active.

Automated math/rasterizer tests do not replace interactive input, display, and shutdown checks.
