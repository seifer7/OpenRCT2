# Temporary 3D view

Keep fork-specific 3D behavior here, not in upstream files. This is an integrated module, not a loadable plugin; a small upstream patch remains necessary.

## Boundaries

- Controller.*: UI-owned session lifetime, SDL timing/key mapping, visible-cursor right-drag camera input, events, restoration, error handling, and frame orchestration.
- Camera.h, Geometry.h: independent camera math and reference clipping/projection helpers.
- Renderer.*: shared box/quad submission and the software reference renderer used by existing tests. The live view does not use its CPU pixel/depth buffers.
- OpenGLRenderer.*: live triangle submission, vertex buffers, shaders, GPU clipping/rasterization, and depth testing.
- MapAdapter.*: live map/tile access, initial camera framing, and terrain/water/placeholder geometry submission.
- TerrainGeometry.h: upstream slope flags and corner heights translated into world-coordinate vertices.
- TileTexture.h: sprite decoding, isometric-to-square texture extraction, material caching, and path surface layout helpers.
- Presentation.*: GPU presentation delegation and the controls label; no dynamic sprite tiles or completed-frame CPU image uploads.

The controller supplies the camera; the map adapter submits geometry; the OpenGL engine delegates the 3D pass into its palette framebuffer before drawing the controls label. Keep compatibility with upstream map/drawing API changes in the adapters rather than spreading it throughout the renderer.

World X/Y/Z coordinates, terrain center-fan triangulation, both valley diagonals, steep slopes, placeholder rules, and camera movement are preserved by this separation.

## Land and path materials

Land reuses the loaded terrain object's flat sprite, including tile variations, grass length, and colour remapping. Its diamond is unprojected into a 32x32 palette texture and mapped onto the existing terrain mesh, including slopes and valleys.

Unprojection uses native diamond scanline coverage, including the second horizontal pixel when the first is transparent, so connected path/queue edges do not acquire transparent seams. Intentional path verges remain transparent.

3D gridlines reuse the existing `ShortcutId::kViewToggleGridlines` binding and action (default `7`), the main viewport's gridline flag, and the native terrain grid sprites. Rebinding the normal gridlines shortcut also changes it in 3D; the overlay displays the current binding. Key repeats do not retrigger the toggle. There is no separate 3D gridlines shortcut.

Paths use their actual surface descriptor, including legacy queue surfaces. Flat path sprites preserve connections, filled corners, and transparent margins. Ramps use the corresponding straight flat texture draped over the proper slope, one world unit above the base to avoid coplanar terrain. Path railings/supports, land cliff textures, and terrain-edge blending are not reproduced yet; other objects remain placeholders.

Decoded materials and GPU texture-array layers are session-owned and reused across frames. Changed sprite storage/metadata triggers re-decoding. Texture uploads occur only for new/changed materials or array growth; the live renderer still rasterizes triangles on the GPU. Missing sprites retain the solid-colour fallback. No game artwork is copied into the repository.

## Translucent water

Water reuses `SPR_WATER_MASK`, the transparent `SPR_WATER_OVERLAY`, and the game's `paletteWater` blend maps. Mask pixels select destination-dependent palette transformations, not opaque blue colours. The existing palette animation supplies the native waves and sparkles, including the active water object's palette. Water in the 3D view is always translucent, without changing the normal game's transparency setting.

Opaque geometry is drawn first, then its palette framebuffer is copied on the GPU for the water shader to sample. The original depth buffer clips water against shorelines and objects above the surface; submerged terrain remains visible through the water tint. Water depth writes retain the nearest surface without repeatedly tinting overlapping tiles. No colour-index arithmetic or unsupported alpha blending into an integer framebuffer is used. The snapshot is resized with the viewport and released with the session. This surface pass does not model refraction or volumetric underwater fog.

Reuse the game's existing sprites for all future 3D materials by default, unless explicitly specified otherwise.

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
- Block normal shortcuts except the shared gridlines shortcut, tools, and scrolling while active. After exit, continue blocking until keyboard keys and mouse buttons are released.
- Keep the cursor visible with relative mouse mode disabled. Rotate the camera only for mouse motion with the right button held; ordinary motion and left-dragging must not rotate it.
- Esc and focus loss restore relative mouse mode, cursor visibility, and position. Quit/window events still reach the host.
- Drawing failures release resources, restore input, report the error, and fall back to the normal scene.
- Presentation does not change graphics settings or replace the game window.
- Entry requires the OpenGL drawing engine and a current context. Other backends and builds without OpenGL show an explanatory message without changing input state.

## Build registration and tests

Add UI sources to this directory and View3D.props, not the upstream project. CMake already discovers UI sources recursively; reconfigure after adding/moving files.

Tests and their View3D.props/sources.cmake manifests live in test/tests/view3d/. Camera/rasterizer tests need neither SDL nor the UI library. Terrain tests compare against core map heights. Texture tests cover unprojection, palette/alpha preservation, UVs, path layouts, and ramps. Water tests check blend-map equivalence, missing-palette fallback, native mask levels, and water-plane continuity. Filters: Map3DGeometry.*, Map3DTextures.*, Map3DWater.*, View3DCamera.*, View3DRenderer.*.

After upstream syncing:

1. Review the small integration patch above and adapt changed APIs inside the adapters/controller.
2. Build and run the feature tests plus TileElementsViewTests.*.
3. Check terrain, water, and placeholders with OpenGL. Check that other display backends reject entry without affecting the normal view.
4. Check WASD, visible cursor, right-drag look, Shift, Esc, focus loss, and exit while movement keys are held. Verify ordinary mouse movement and left-dragging do not rotate the camera.
5. Check resizing, repeated entry/exit, restored game tools/shortcuts, and quitting while active.
6. Check translucent water over slopes/valleys, dry shoreline edges, submerged paths, objects above water, different water heights, and wave animation. Verify resizing and Esc restore the normal view without texture/framebuffer state leaking.

Automated math/rasterizer tests do not replace interactive input, display, and shutdown checks.
