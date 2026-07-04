#!/usr/bin/env python3
"""Generate cube.gltf and cube.bin for KengaEngine assets."""
import base64
import json
import struct

# 24 vertices for cube (4 per face), -0.5 to 0.5
positions = [
    (-0.5, -0.5, -0.5), (0.5, -0.5, -0.5), (0.5, 0.5, -0.5), (-0.5, 0.5, -0.5),
    (0.5, -0.5, 0.5), (-0.5, -0.5, 0.5), (-0.5, 0.5, 0.5), (0.5, 0.5, 0.5),
    (-0.5, -0.5, -0.5), (-0.5, -0.5, 0.5), (0.5, -0.5, 0.5), (0.5, -0.5, -0.5),
    (-0.5, 0.5, -0.5), (-0.5, 0.5, 0.5), (0.5, 0.5, 0.5), (0.5, 0.5, -0.5),
    (-0.5, -0.5, -0.5), (-0.5, 0.5, -0.5), (-0.5, 0.5, 0.5), (-0.5, -0.5, 0.5),
    (0.5, -0.5, -0.5), (0.5, 0.5, -0.5), (0.5, 0.5, 0.5), (0.5, -0.5, 0.5),
]
normals = [
    (0, 0, -1), (0, 0, -1), (0, 0, -1), (0, 0, -1),
    (0, 0, 1), (0, 0, 1), (0, 0, 1), (0, 0, 1),
    (0, -1, 0), (0, -1, 0), (0, -1, 0), (0, -1, 0),
    (0, 1, 0), (0, 1, 0), (0, 1, 0), (0, 1, 0),
    (-1, 0, 0), (-1, 0, 0), (-1, 0, 0), (-1, 0, 0),
    (1, 0, 0), (1, 0, 0), (1, 0, 0), (1, 0, 0),
]
indices = [
    0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7,
    8, 9, 10, 8, 10, 11, 12, 13, 14, 12, 14, 15,
    16, 17, 18, 16, 18, 19, 20, 21, 22, 20, 22, 23,
]

buf = b""
for p in positions:
    buf += struct.pack("<3f", *p)
for n in normals:
    buf += struct.pack("<3f", *n)
for i in indices:
    buf += struct.pack("<H", i)

import os
assets_dir = os.path.join(os.path.dirname(__file__), "..", "assets")
os.makedirs(assets_dir, exist_ok=True)

# Write cube.bin
bin_path = os.path.join(assets_dir, "cube.bin")
with open(bin_path, "wb") as f:
    f.write(buf)
print(f"Wrote {bin_path} ({len(buf)} bytes)")

# Write cube.gltf
b64 = base64.b64encode(buf).decode()
gltf = {
    "asset": {"version": "2.0", "generator": "KengaEngine"},
    "scene": 0,
    "scenes": [{"nodes": [0]}],
    "nodes": [{"mesh": 0}],
    "meshes": [{
        "primitives": [{
            "attributes": {"POSITION": 1, "NORMAL": 2},
            "indices": 0,
            "mode": 4,
        }]
    }],
    "accessors": [
        {"bufferView": 0, "byteOffset": 0, "componentType": 5123, "count": 36, "type": "SCALAR"},
        {"bufferView": 1, "byteOffset": 0, "componentType": 5126, "count": 24, "type": "VEC3"},
        {"bufferView": 1, "byteOffset": 288, "componentType": 5126, "count": 24, "type": "VEC3"},
    ],
    "bufferViews": [
        {"buffer": 0, "byteOffset": 576, "byteLength": 72, "target": 34963},
        {"buffer": 0, "byteOffset": 0, "byteLength": 576, "target": 34962},
    ],
    "buffers": [
        {"byteLength": 648, "uri": "cube.bin"},
    ],
}

gltf_path = os.path.join(assets_dir, "cube.gltf")
with open(gltf_path, "w") as f:
    json.dump(gltf, f, indent=2)
print(f"Wrote {gltf_path}")
