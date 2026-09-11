"""Save an isolated Blender asset using the image-to-blender placement contract."""

import json
import math
import os
from pathlib import Path
import struct
import tempfile

import bpy
from mathutils import Matrix, Vector


def _is_identity(matrix):
    identity = Matrix.Identity(4)
    return all(
        math.isfinite(matrix[row][column])
        and abs(matrix[row][column] - identity[row][column]) <= 1e-6
        for row in range(4)
        for column in range(4)
    )


def _read_glb(path):
    data = path.read_bytes()
    if len(data) < 20:
        raise RuntimeError("The GLB export is truncated.")
    magic, version, size, json_size, chunk_type = struct.unpack_from("<4s4I", data)
    if magic != b"glTF" or version != 2 or size != len(data) or chunk_type != 0x4E4F534A:
        raise RuntimeError("The exporter did not produce a valid glTF 2 binary header.")
    document = json.loads(data[20 : 20 + json_size])
    if any("uri" in item for item in document.get("buffers", [])):
        raise RuntimeError("The GLB unexpectedly references an external buffer.")
    if any("bufferView" not in image for image in document.get("images", [])):
        raise RuntimeError("The GLB contains an image that is not embedded.")
    return document


def save_asset(
    *,
    root_name,
    source_image,
    image_name,
    output_directory,
    footprint_tiles,
    overwrite=False,
):
    """Save <original image stem>.blend and .glb; never infer scale or ground anchors."""
    source_image = Path(source_image).resolve()
    if not source_image.is_file():
        raise ValueError(f"Source image does not exist: {source_image}")
    if not image_name or any(char in image_name for char in '/\\<>:"|?*'):
        raise ValueError("image_name must be the original image filename, not a path.")
    name = Path(image_name)
    if not name.suffix or name.suffix.lower() in {".blend", ".glb"} or not name.stem.strip(". "):
        raise ValueError("image_name must include an image extension and a nonempty stem.")
    if len(footprint_tiles) != 2:
        raise ValueError("footprint_tiles must contain the X and Y placement spans.")
    footprint = tuple(float(value) for value in footprint_tiles)
    if any(not math.isfinite(value) or value <= 0 for value in footprint):
        raise ValueError("Both footprint tile spans must be finite and positive.")

    scene = bpy.context.scene
    if bpy.context.mode != "OBJECT":
        raise ValueError("Finish editing and enter Object Mode before exporting.")
    if not math.isclose(scene.unit_settings.scale_length, 1.0, rel_tol=0, abs_tol=1e-6):
        raise ValueError("Scene unit scale must be 1.0; each tile is four mesh-space units.")
    root = scene.objects.get(root_name)
    if root is None or root.type not in {"EMPTY", "MESH"} or root.parent is not None:
        raise ValueError("The asset root must be an unparented mesh or empty in the active scene.")
    bpy.context.view_layer.update()
    if not _is_identity(root.matrix_basis) or not _is_identity(root.matrix_world):
        raise ValueError("The asset root must have zero translation/rotation and unit scale.")
    if root.constraints or root.animation_data is not None:
        raise ValueError("Keep the placement root static; put constraints and animation on children.")
    transforms = (
        (root.location, (0, 0, 0)),
        (root.scale, (1, 1, 1)),
        (root.delta_location, (0, 0, 0)),
        (root.delta_scale, (1, 1, 1)),
        (root.delta_rotation_euler, (0, 0, 0)),
        (root.delta_rotation_quaternion, (1, 0, 0, 0)),
    )
    if any(
        not math.isfinite(value) or abs(value - expected) > 1e-6
        for values, defaults in transforms
        for value, expected in zip(values, defaults)
    ):
        raise ValueError("Do not cancel nonzero root transforms with delta transforms.")

    objects = [root, *root.children_recursive]
    if not any(obj.type == "MESH" and obj.data.polygons for obj in objects):
        raise ValueError("The asset hierarchy must contain mesh faces.")
    for obj in objects:
        if obj.type not in {"EMPTY", "MESH", "ARMATURE"}:
            raise ValueError(f"Convert or move unsupported asset object {obj.name!r} ({obj.type}).")
        if obj.name not in bpy.context.view_layer.objects or not obj.visible_get() or obj.hide_select:
            raise ValueError(f"Asset object {obj.name!r} must be visible and selectable in this view layer.")

    depsgraph = bpy.context.evaluated_depsgraph_get()
    points = []
    for obj in objects:
        if obj.type == "MESH":
            evaluated = obj.evaluated_get(depsgraph)
            points.extend(evaluated.matrix_world @ Vector(corner) for corner in evaluated.bound_box)
    if not points or any(not math.isfinite(value) for point in points for value in point):
        raise ValueError("The asset must have finite geometry bounds.")
    bounds_min = tuple(min(point[axis] for point in points) for axis in range(3))
    bounds_max = tuple(max(point[axis] for point in points) for axis in range(3))
    if bounds_max[2] <= 0:
        raise ValueError("Visible geometry must extend above the Z = 0 ground plane.")

    output_directory = Path(output_directory).resolve()
    output_directory.mkdir(parents=True, exist_ok=True)
    blend_path = output_directory / f"{name.stem}.blend"
    glb_path = output_directory / f"{name.stem}.glb"
    for path in (blend_path, glb_path):
        if path.exists() and not overwrite:
            raise FileExistsError(f"Refusing to overwrite {path}; obtain permission first.")

    root["tile_size_blender_units"] = 4.0
    root["footprint_tiles"] = footprint
    root["source_image_name"] = image_name
    root["blender_front_axis"] = "-Y"
    selected = list(bpy.context.selected_objects)
    active = bpy.context.view_layer.objects.active
    try:
        bpy.ops.object.select_all(action="DESELECT")
        for obj in objects:
            obj.select_set(True)
        bpy.context.view_layer.objects.active = root
        if set(bpy.context.selected_objects) != set(objects):
            raise RuntimeError("Could not select exactly the asset hierarchy for export.")
        if bpy.ops.file.pack_all() != {"FINISHED"}:
            raise RuntimeError("Blender could not pack the asset dependencies.")
        for image in bpy.data.images:
            if image.source == "FILE" and not image.packed_file:
                raise RuntimeError(f"Image {image.name!r} could not be packed.")

        with tempfile.TemporaryDirectory(prefix=".image-to-blender-", dir=output_directory) as staging:
            staged_blend = Path(staging) / blend_path.name
            staged_glb = Path(staging) / glb_path.name
            status = bpy.ops.export_scene.gltf(
                filepath=str(staged_glb),
                export_format="GLB",
                export_yup=True,
                use_selection=True,
                export_cameras=False,
                export_lights=False,
                export_extras=True,
                export_animations=True,
                export_materials="EXPORT",
            )
            if status != {"FINISHED"}:
                raise RuntimeError(f"GLB export failed: {status}")
            document = _read_glb(staged_glb)
            nodes = document.get("nodes", [])
            roots = document["scenes"][document.get("scene", 0)].get("nodes", [])
            if len(roots) != 1 or nodes[roots[0]].get("name") != root.name:
                raise RuntimeError("The GLB must contain exactly the selected placement root.")
            if bpy.ops.wm.save_as_mainfile(filepath=str(staged_blend), copy=True) != {"FINISHED"}:
                raise RuntimeError("Saving the native Blender copy failed.")
            if not staged_blend.is_file() or staged_blend.stat().st_size == 0:
                raise RuntimeError("Blender did not save a nonempty native file.")
            # Stage both files first; report filesystem publication errors rather than hiding them.
            for staged, destination in ((staged_blend, blend_path), (staged_glb, glb_path)):
                if overwrite:
                    os.replace(staged, destination)
                else:
                    with destination.open("xb") as output:
                        output.write(staged.read_bytes())
    finally:
        bpy.ops.object.select_all(action="DESELECT")
        for obj in selected:
            obj.select_set(True)
        bpy.context.view_layer.objects.active = active

    return {
        "blend": str(blend_path),
        "glb": str(glb_path),
        "footprint_tiles": footprint,
        "footprint_blender_units": tuple(value * 4 for value in footprint),
        "bounds_min": bounds_min,
        "bounds_max": bounds_max,
        "dimensions": tuple(high - low for low, high in zip(bounds_min, bounds_max)),
    }
