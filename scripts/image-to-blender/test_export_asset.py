"""Run with Blender --background --factory-startup --python-exit-code 1 --python."""

from pathlib import Path
import runpy
import tempfile
import unittest

import bpy
from mathutils import Vector


HELPER = runpy.run_path(str(Path(__file__).with_name("export_asset.py")))
save_asset = HELPER["save_asset"]


class ExportAssetTests(unittest.TestCase):
    def setUp(self):
        bpy.ops.wm.read_factory_settings(use_empty=True)
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        self.source = self.directory / "transport-uuid-Oak Tree.png"
        image = bpy.data.images.new("Painted atlas", width=2, height=2)
        image.pixels[:] = [0.2, 0.5, 0.1, 1.0] * 4
        image.filepath_raw = str(self.source)
        image.file_format = "PNG"
        image.save()
        self.root = bpy.data.objects.new("Oak Tree", None)
        bpy.context.scene.collection.objects.link(self.root)
        bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 3))
        self.mesh = bpy.context.object
        self.mesh.name = "Body"
        self.mesh.parent = self.root
        self.mesh.scale = (8, 4, 6)
        bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
        material = bpy.data.materials.new("Painted")
        material.use_nodes = True
        texture = material.node_tree.nodes.new("ShaderNodeTexImage")
        texture.image = image
        texture.interpolation = "Closest"
        material.node_tree.links.new(
            texture.outputs["Color"],
            material.node_tree.nodes["Principled BSDF"].inputs["Base Color"],
        )
        self.mesh.data.materials.append(material)
        self.pivot = bpy.data.objects.new("Front pivot", None)
        bpy.context.scene.collection.objects.link(self.pivot)
        self.pivot.parent = self.root
        self.pivot.location = (1, -3, 2)
        bpy.ops.mesh.primitive_cube_add(size=0.5)
        marker = bpy.context.object
        marker.name = "Front marker"
        marker.parent = self.pivot
        marker.location = (0, 0, 0)
        bpy.ops.mesh.primitive_plane_add(size=40, location=(0, 0, -1))
        bpy.context.object.name = "Preview floor"

    def export(self, **kwargs):
        return save_asset(
            root_name=self.root.name,
            source_image=self.source,
            image_name="Oak Tree.png",
            output_directory=self.directory / "output",
            footprint_tiles=kwargs.pop("footprint_tiles", (2, 1)),
            **kwargs,
        )

    def assert_vector(self, actual, expected):
        for value, target in zip(actual, expected):
            self.assertAlmostEqual(value, target, places=5)

    def test_export_and_round_trip(self):
        original_filepath = bpy.data.filepath
        selected = set(bpy.context.selected_objects)
        outputs = self.export()
        self.assertEqual(bpy.data.filepath, original_filepath)
        self.assertEqual(set(bpy.context.selected_objects), selected)
        self.assertEqual(Path(outputs["blend"]).name, "Oak Tree.blend")
        self.assertEqual(Path(outputs["glb"]).name, "Oak Tree.glb")
        self.assertEqual(outputs["footprint_blender_units"], (8, 4))
        self.assertEqual(outputs["bounds_min"][2], 0)
        document = HELPER["_read_glb"](Path(outputs["glb"]))
        nodes = {node["name"]: node for node in document["nodes"]}
        self.assertNotIn("Preview floor", nodes)
        self.assertNotIn("cameras", document)
        self.assertEqual(len(document["scenes"][0]["nodes"]), 1)
        self.assert_vector(nodes["Front pivot"]["translation"], (1, 2, 3))
        self.assertNotIn("rotation", nodes["Oak Tree"])
        self.assertNotIn("translation", nodes["Oak Tree"])
        self.assertNotIn("scale", nodes["Oak Tree"])
        self.assertEqual(nodes["Oak Tree"]["extras"]["tile_size_blender_units"], 4)
        self.assertTrue(document["images"])
        self.assertTrue(all("bufferView" in image for image in document["images"]))
        self.assertEqual(document["samplers"][0]["magFilter"], 9728)

        bpy.ops.wm.open_mainfile(filepath=outputs["blend"])
        self.assertEqual(bpy.data.objects["Body"].parent.name, "Oak Tree")
        self.assert_vector(bpy.data.objects["Front pivot"].location, (1, -3, 2))
        self.assertTrue(bpy.data.images["Painted atlas"].packed_file)
        self.assertEqual(bpy.context.scene.unit_settings.scale_length, 1)
        self.assert_vector(bpy.data.objects["Oak Tree"].location, (0, 0, 0))

        bpy.ops.wm.read_factory_settings(use_empty=True)
        bpy.ops.import_scene.gltf(filepath=outputs["glb"])
        imported_root = bpy.data.objects["Oak Tree"]
        self.assert_vector(imported_root.location, (0, 0, 0))
        self.assert_vector(imported_root.scale, (1, 1, 1))
        self.assert_vector(bpy.data.objects["Front pivot"].location, (1, -3, 2))
        self.assert_vector(bpy.data.objects["Body"].dimensions, (8, 4, 6))
        points = [
            obj.matrix_world @ Vector(corner)
            for obj in bpy.context.scene.objects if obj.type == "MESH"
            for corner in obj.bound_box
        ]
        self.assert_vector(
            tuple(min(point[axis] for point in points) for axis in range(3)),
            outputs["bounds_min"],
        )
        self.assert_vector(
            tuple(max(point[axis] for point in points) for axis in range(3)),
            outputs["bounds_max"],
        )

    def test_rejects_root_translation_rotation_and_scale(self):
        for attribute, value in (
            ("location", (0, 0, 1)),
            ("rotation_euler", (0, 0, 0.5)),
            ("scale", (4, 4, 4)),
        ):
            with self.subTest(attribute=attribute):
                original = getattr(self.root, attribute).copy()
                setattr(self.root, attribute, value)
                with self.assertRaisesRegex(ValueError, "zero translation/rotation"):
                    self.export()
                setattr(self.root, attribute, original)

    def test_rejects_scaled_scene_and_invalid_footprint(self):
        bpy.context.scene.unit_settings.scale_length = 0.01
        with self.assertRaisesRegex(ValueError, "Scene unit scale"):
            self.export()
        bpy.context.scene.unit_settings.scale_length = 1
        for footprint in ((0, 1), (-1, 1), (float("nan"), 1), (1,)):
            with self.subTest(footprint=footprint), self.assertRaises(ValueError):
                self.export(footprint_tiles=footprint)

    def test_collision_leaves_existing_files_unchanged(self):
        outputs = self.export()
        before = {path: Path(path).read_bytes() for path in (outputs["blend"], outputs["glb"])}
        with self.assertRaises(FileExistsError):
            self.export()
        for path, content in before.items():
            self.assertEqual(Path(path).read_bytes(), content)
        self.export(overwrite=True)

    def test_rejects_helpers_inside_root(self):
        bpy.ops.object.camera_add()
        bpy.context.object.parent = self.root
        with self.assertRaisesRegex(ValueError, "unsupported asset object"):
            self.export()

    def test_rejects_animated_placement_root(self):
        self.root.keyframe_insert(data_path="location", frame=1)
        with self.assertRaisesRegex(ValueError, "placement root static"):
            self.export()

    def test_preserves_child_animation(self):
        scene = bpy.context.scene
        scene.frame_start = 1
        scene.frame_end = 4
        self.pivot.keyframe_insert(data_path="location", frame=1)
        self.pivot.location.z = 3
        self.pivot.keyframe_insert(data_path="location", frame=4)
        scene.frame_set(1)
        outputs = self.export()
        document = HELPER["_read_glb"](Path(outputs["glb"]))
        pivot_index = next(
            index for index, node in enumerate(document["nodes"]) if node["name"] == "Front pivot"
        )
        self.assertTrue(any(
            channel["target"]["node"] == pivot_index and channel["target"]["path"] == "translation"
            for animation in document["animations"]
            for channel in animation["channels"]
        ))
        bpy.ops.wm.read_factory_settings(use_empty=True)
        bpy.ops.import_scene.gltf(filepath=outputs["glb"])
        bpy.context.scene.frame_set(1)
        self.assert_vector(bpy.data.objects["Front pivot"].location, (1, -3, 2))
        bpy.context.scene.frame_set(4)
        self.assert_vector(bpy.data.objects["Front pivot"].location, (1, -3, 3))

    def test_single_mesh_root_and_foundation(self):
        self.mesh.parent = None
        for vertex in self.mesh.data.vertices:
            vertex.co.z += 3
            if vertex.co.z == 0:
                vertex.co.z = -1
        self.mesh.location = (0, 0, 0)
        self.root = self.mesh
        outputs = self.export(footprint_tiles=(2, 0.5))
        self.assertEqual(outputs["footprint_blender_units"], (8, 2))
        self.assertEqual(outputs["bounds_min"][2], -1)
        document = HELPER["_read_glb"](Path(outputs["glb"]))
        self.assertEqual(len(document["nodes"]), 1)
        self.assertEqual(document["nodes"][0]["name"], "Body")
        self.assertNotIn("rotation", document["nodes"][0])
        self.assertNotIn("translation", document["nodes"][0])
        self.assertNotIn("scale", document["nodes"][0])

    def test_copy_preserves_open_native_filepath(self):
        original = self.directory / "Editable source.blend"
        bpy.ops.wm.save_as_mainfile(filepath=str(original))
        original_bytes = original.read_bytes()
        outputs = self.export()
        self.assertEqual(bpy.data.filepath, str(original))
        self.assertEqual(original.read_bytes(), original_bytes)
        self.assertNotEqual(outputs["blend"], str(original))


if __name__ == "__main__":
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(ExportAssetTests)
    if not unittest.TextTestRunner(verbosity=2).run(suite).wasSuccessful():
        raise SystemExit(1)
