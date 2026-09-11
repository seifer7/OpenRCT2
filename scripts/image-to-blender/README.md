# Image-to-Blender agent

The repository's `image-to-blender` custom agent reconstructs sprite-faithful
Blender assets from four-angle isometric reference images. It is separate from
the game's 3D renderer and does not change sprite rendering or register models
with the game.

## Use

Open this worktree in a Copilot client that supports repository custom agents
and select **image-to-blender**. In Copilot CLI, use `/agent` to select it; start
a new session if a running client has not discovered the new profile. The
profile lives in `.github/agents/image-to-blender.agent.md`. It inherits the
session's model and available tools, including an already configured Blender
MCP server; it does not install a server or provision Blender.

Attach the four-view source image and give an output folder, for example:

> Create a model from `Oak Tree.png` using the bundled style reference. Save
> `Oak Tree.blend` and `Oak Tree.glb` in my chosen asset folder.

The top image is always the front view. Other views are matched by landmarks,
not an assumed clockwise order. A complete grid tile is four Blender units
along either ground axis. The native model uses +Z up, -Y front, and +X right;
its placement root is at the functional ground anchor with identity transforms.
The GLB exporter performs Y-up conversion without an extra root rotation.

Blender with its glTF importer/exporter is required. The agent can use Blender
MCP or an installed Blender executable with background Python. Reference
inspection and generation should run in separate processes so an unrelated
open Blender document is not changed. Work is local; no external generation
service or asset upload is part of this workflow.

This is an agent-guided reconstruction process, not a trained image-to-mesh
model or a guarantee of automatic pixel-perfect geometry. The agent must
measure the grid, compare renders, and ask about genuinely ambiguous features.

## Bundled style reference

`references/isometric sample.png` and `references/ParkEntrance.glb` are the
user-supplied design/style example, retained unchanged for future sessions.
They are reference inputs, not generated deliverables or a runtime asset
registration. No additional rights or license for these supplied references
are asserted by this workflow; do not redistribute them independently without
the appropriate permission.

The inspected GLB has one textured mesh, 20,666 triangles, a 2048-by-2048 color
atlas, predominantly smooth shading, zero metallic factor, and roughness near
0.8. Visually it uses simplified, slightly irregular forms and painted details:
cream/brown masonry, terracotta spires, green domes, yellow figures, and a
banner. Preserve that hand-crafted, sprite-era character rather than imposing
photorealism, uniformly flat shading, voxels, or a triangle/texture-size quota.

**Its transforms and dimensions are not a template.** Imported into Blender,
the example is approximately 0.705 by 0.244 by 0.573 units and has a nonzero
mesh-origin height despite geometry touching ground. Every new asset must be
calibrated independently to four units per source tile and use the requested
ground-level placement root.

## Export helper

`export_asset.py` runs inside Blender, not system Python. Call it after the
model and four-view comparisons are finished, in the isolated asset document:

```python
import runpy
from pathlib import Path

workspace = Path(r"C:\path\to\this\worktree")
save_asset = runpy.run_path(
    str(workspace / "scripts" / "image-to-blender" / "export_asset.py")
)["save_asset"]

outputs = save_asset(
    root_name="Oak Tree",
    source_image=Path(r"C:\references\Oak Tree.png"),
    image_name="Oak Tree.png",  # Original attachment name, not its storage UUID.
    output_directory=Path(r"C:\assets"),
    footprint_tiles=(1, 1),
)
print(outputs)
```

The function requires one unparented mesh or empty as the asset root, with only
meshes, empties, and armatures in its subtree. The root must have identity
transforms, no constraints/animation, and the scene must have unit scale 1.
The asset must be visible/selectable in the active view layer. Child transforms
and animation pivots are preserved. `footprint_tiles` describes the intended
placement footprint, not the visible bounding box; fractional positive tile
spans are supported.

The helper records `tile_size_blender_units`, `footprint_tiles`,
`source_image_name`, and `blender_front_axis` on the root and exports these as
glTF extras. The last property describes the **native** convention, not the
converted GLB direction.

It stages both outputs before publishing them and refuses existing target
files unless `overwrite=True` is explicitly authorized. It restores selection
afterward and saves the native file with Blender's `copy=True`; it does not
replace the currently opened document's filepath. Metadata changes and texture
packing do modify that in-memory asset document, which is another reason to
use an isolated Blender process.

The helper checks mechanical invariants, not image interpretation. It cannot
prove footprint centering, semantic ground contact, front-facing design,
tile measurements, or stylistic similarity. Those require the agent's grid
measurements, landmark checks, four-view comparison, and clean-file round trip.

## Helper checks

Run the focused smoke tests using your installed Blender executable:

```powershell
& 'C:\path\to\blender.exe' --background --factory-startup --python-exit-code 1 --python .\scripts\image-to-blender\test_export_asset.py
```

The tests use temporary files and synthetic geometry. They cover paired
filenames, packed texture export, root/scene-unit guardrails, output collisions,
an asymmetric asset with a local animated pivot, preview-object exclusion,
native file reopening, preserving the open document's filepath, a single-mesh
root with underground foundations, fractional tile spans, and GLB Y-up
conversion with dimension-preserving round trip.
