#!/usr/bin/env python3
"""Deterministic STL fixture corpus for the conversion regression suite.

Extends the attached generator (20 mm cube binary + ASCII, cylinder d20 h40,
torus, sphere r25, two boxes, open cube, 0.1 mm cube, 200k-triangle sphere,
invalid non-STL) with edge cases. Small files and manifest.json are written
next to this script. The 200k-triangle sphere is not committed; pass
--large-dir to write it (the CTest fixture does this at test time).

Usage:
  python3 gen_fixtures.py                 # rewrite committed fixtures + manifest
  python3 gen_fixtures.py --check --large-dir DIR
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import struct
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
ASSETS = HERE.parents[2] / "app" / "assets" / "regression"
ASSET_NAMES = (
    "01_cube_20mm_binary.stl",
    "02_cube_20mm_ascii.stl",
    "07_open_mesh_cube_missing_top.stl",
    "10_invalid_not_stl.stl",
)
LARGE_NAME = "09_large_sphere_200k_tris.stl"


def f32(x: float) -> float:
    return struct.unpack("<f", struct.pack("<f", float(x)))[0]


def qpoint(p):
    return (f32(p[0]), f32(p[1]), f32(p[2]))


def qtris(tris):
    return [tuple(qpoint(p) for p in t) for t in tris]


def norm(a, b, c):
    u = [b[i] - a[i] for i in range(3)]
    v = [c[i] - a[i] for i in range(3)]
    n = [u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]]
    length = math.sqrt(sum(x * x for x in n)) or 1.0
    return [x / length for x in n]


def mesh_volume(tris) -> float:
    volume = 0.0
    for a, b, c in tris:
        volume += (
            a[0] * (b[1] * c[2] - b[2] * c[1])
            - a[1] * (b[0] * c[2] - b[2] * c[0])
            + a[2] * (b[0] * c[1] - b[1] * c[0])
        ) / 6.0
    return volume


def box(sx, sy, sz, ox=0.0, oy=0.0, oz=0.0):
    v = [(ox + x * sx, oy + y * sy, oz + z * sz) for x in (0, 1) for y in (0, 1) for z in (0, 1)]
    quads = [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)]
    tris = []
    for a, b, c, d in quads:
        tris.append((v[a], v[b], v[c]))
        tris.append((v[a], v[c], v[d]))
    return tris


def subdivided_box(sx, sy, sz, n, ox=0.0, oy=0.0, oz=0.0):
    """Closed box with n x n quads per face. STL stores each corner many times."""
    tris = []
    for axis in range(3):
        for side in range(2):
            u = (axis + 1) % 3
            v = (axis + 2) % 3

            def at(i, j, axis=axis, side=side, u=u, v=v):
                p = [0.0, 0.0, 0.0]
                p[axis] = float(side)
                p[u] = i / n
                p[v] = j / n
                scale = (sx, sy, sz)
                origin = (ox, oy, oz)
                return tuple(origin[k] + p[k] * scale[k] for k in range(3))

            for i in range(n):
                for j in range(n):
                    a = [at(i, j), at(i + 1, j), at(i + 1, j + 1)]
                    b = [at(i, j), at(i + 1, j + 1), at(i, j + 1)]
                    if side == 0:
                        a = [a[0], a[2], a[1]]
                        b = [b[0], b[2], b[1]]
                    tris.append(tuple(a))
                    tris.append(tuple(b))
    return tris


def sphere(r, nu, nv):
    tris = []

    def point(i, j):
        return (
            r * math.sin(math.pi * j / nv) * math.cos(2 * math.pi * i / nu),
            r * math.sin(math.pi * j / nv) * math.sin(2 * math.pi * i / nu),
            r * math.cos(math.pi * j / nv),
        )

    for i in range(nu):
        for j in range(nv):
            a, b, c, d = point(i, j), point(i + 1, j), point(i + 1, j + 1), point(i, j + 1)
            if j > 0:
                tris.append((a, c, b))
            if j < nv - 1:
                tris.append((a, d, c))
    return tris


def cyl(r, h, n):
    tris = []
    for i in range(n):
        a0 = 2 * math.pi * i / n
        a1 = 2 * math.pi * (i + 1) / n
        p0 = (r * math.cos(a0), r * math.sin(a0))
        p1 = (r * math.cos(a1), r * math.sin(a1))
        tris.append(((0, 0, 0), (*p1, 0), (*p0, 0)))
        tris.append(((0, 0, h), (*p0, h), (*p1, h)))
        tris.append(((*p0, 0), (*p1, 0), (*p1, h)))
        tris.append(((*p0, 0), (*p1, h), (*p0, h)))
    return tris


def torus(big_r, small_r, nu, nv):
    def point(i, j):
        return (
            (big_r + small_r * math.cos(2 * math.pi * j / nv)) * math.cos(2 * math.pi * i / nu),
            (big_r + small_r * math.cos(2 * math.pi * j / nv)) * math.sin(2 * math.pi * i / nu),
            small_r * math.sin(2 * math.pi * j / nv),
        )

    tris = []
    for i in range(nu):
        for j in range(nv):
            a, b, c, d = point(i, j), point(i + 1, j), point(i + 1, j + 1), point(i, j + 1)
            tris.append((a, b, c))
            tris.append((a, c, d))
    return tris


def invert(tris):
    return [(a, c, b) for a, b, c in tris]


def quad(a, b, c, d):
    """Two triangles. Normal is (b-a) x (d-a)."""
    return [(a, b, c), (a, c, d)]


def through_hole():
    """40 x 30 x 20 block with a 10 x 10 square hole along Z. Volume 22000.

    The surface is a 3x3 grid so every shared edge uses the same vertices
    (a coarser annulus leaves T-junctions and the core correctly refuses a solid).
    """
    xs = (0.0, 15.0, 25.0, 40.0)
    ys = (0.0, 10.0, 20.0, 30.0)
    z0, z1 = 0.0, 20.0

    def point(i, j, z):
        return (xs[i], ys[j], z)

    tris = []

    def add(a, b, c, d):
        tris.extend(quad(a, b, c, d))

    for i in range(3):
        for j in range(3):
            if i == 1 and j == 1:
                continue
            add(point(i, j, z1), point(i + 1, j, z1), point(i + 1, j + 1, z1), point(i, j + 1, z1))
            add(point(i, j, z0), point(i, j + 1, z0), point(i + 1, j + 1, z0), point(i + 1, j, z0))
    for j in range(3):
        add(point(0, j, z0), point(0, j, z1), point(0, j + 1, z1), point(0, j + 1, z0))
        add(point(3, j, z0), point(3, j + 1, z0), point(3, j + 1, z1), point(3, j, z1))
    for i in range(3):
        add(point(i, 0, z0), point(i + 1, 0, z0), point(i + 1, 0, z1), point(i, 0, z1))
        add(point(i, 3, z0), point(i, 3, z1), point(i + 1, 3, z1), point(i + 1, 3, z0))
    add(point(1, 1, z0), point(1, 2, z0), point(1, 2, z1), point(1, 1, z1))
    add(point(2, 1, z0), point(2, 1, z1), point(2, 2, z1), point(2, 2, z0))
    add(point(1, 1, z0), point(1, 1, z1), point(2, 1, z1), point(2, 1, z0))
    add(point(1, 2, z0), point(2, 2, z0), point(2, 2, z1), point(1, 2, z1))
    return tris


def boundary_edges(tris):
    """Count edges used once or more than twice after exact float32 welding."""
    lookup = {}
    points = []
    uses = {}
    for tri in tris:
        index = []
        for point in tri:
            if point not in lookup:
                lookup[point] = len(points)
                points.append(point)
            index.append(lookup[point])
        if len(set(index)) < 3:
            continue
        a, b, c = tri
        ux, uy, uz = b[0] - a[0], b[1] - a[1], b[2] - a[2]
        vx, vy, vz = c[0] - a[0], c[1] - a[1], c[2] - a[2]
        nx = uy * vz - uz * vy
        ny = uz * vx - ux * vz
        nz = ux * vy - uy * vx
        if not (nx * nx + ny * ny + nz * nz > 0):
            continue
        for u, v in ((index[0], index[1]), (index[1], index[2]), (index[2], index[0])):
            edge = (u, v) if u < v else (v, u)
            uses[edge] = uses.get(edge, 0) + 1
    boundary = sum(1 for count in uses.values() if count == 1)
    nonmanifold = sum(1 for count in uses.values() if count != 2)
    return boundary, nonmanifold, len(points), len(uses), len(tris)


def wb(path: Path, tris) -> None:
    with path.open("wb") as handle:
        handle.write(b"stl2stp test".ljust(80, b" "))
        handle.write(struct.pack("<I", len(tris)))
        for tri in tris:
            handle.write(struct.pack("<3f", *norm(*tri)))
            for point in tri:
                handle.write(struct.pack("<3f", *point))
            handle.write(b"\0\0")


def wa(path: Path, tris, name: str) -> None:
    with path.open("w", encoding="ascii", newline="\n") as handle:
        handle.write(f"solid {name}\n")
        for tri in tris:
            n = norm(*tri)
            handle.write(f" facet normal {n[0]:e} {n[1]:e} {n[2]:e}\n  outer loop\n")
            for point in tri:
                handle.write(f"   vertex {point[0]:e} {point[1]:e} {point[2]:e}\n")
            handle.write("  endloop\n endfacet\n")
        handle.write(f"endsolid {name}\n")


def _ascii_body(tris, name: str) -> str:
    lines = [f"solid\t{name}\r\n\r\n"]
    for tri in tris:
        n = norm(*tri)
        lines.append(f"  facet   normal   {n[0]:e}  {n[1]:e}  {n[2]:e}\r\n")
        lines.append("    outer    loop\r\n")
        for point in tri:
            lines.append(f"\t  vertex\t{point[0]:e}   {point[1]:e}   {point[2]:e}\r\n")
        lines.append("    endloop\r\n")
        lines.append("  endfacet\r\n\r\n")
    lines.append(f"endsolid {name}\r\n")
    return "".join(lines)


def wa_crlf(path: Path, tris, name: str) -> None:
    # First five bytes must be "solid" or the loader's binary detector claims the file.
    path.write_bytes(_ascii_body(tris, name).encode("ascii"))


def wa_leading_space(path: Path, tris, name: str) -> None:
    path.write_bytes(("  \r\n" + _ascii_body(tris, name)).encode("ascii"))


def write_truncated(path: Path, tris, declared: int, extra: bytes) -> None:
    blob = bytearray()
    blob += b"stl2stp test".ljust(80, b" ")
    blob += struct.pack("<I", declared)
    for tri in tris:
        blob += struct.pack("<3f", *norm(*tri))
        for point in tri:
            blob += struct.pack("<3f", *point)
        blob += b"\0\0"
    blob += extra
    path.write_bytes(blob)


def loader_model(tris):
    """Mirror mesh.cpp: exact-coordinate weld, drop repeated-vertex and zero-area facets."""
    lookup = {}
    points = []
    kept = []
    skipped = 0
    for tri in tris:
        index = []
        for point in tri:
            if point not in lookup:
                lookup[point] = len(points)
                points.append(point)
            index.append(lookup[point])
        if len(set(index)) < 3:
            skipped += 1
            continue
        a, b, c = tri
        ux, uy, uz = b[0] - a[0], b[1] - a[1], b[2] - a[2]
        vx, vy, vz = c[0] - a[0], c[1] - a[1], c[2] - a[2]
        nx = uy * vz - uz * vy
        ny = uz * vx - ux * vz
        nz = ux * vy - uy * vx
        if not (nx * nx + ny * ny + nz * nz > 0):
            skipped += 1
            continue
        kept.append(tri)
    if points:
        mins = [min(p[i] for p in points) for i in range(3)]
        maxs = [max(p[i] for p in points) for i in range(3)]
    else:
        mins = [0.0, 0.0, 0.0]
        maxs = [0.0, 0.0, 0.0]
    return {
        "triangles": len(kept),
        "vertices": len(points),
        "skipped": skipped,
        "bbox_min": mins,
        "bbox_max": maxs,
        "volume": mesh_volume(kept),
        "records": len(tris),
    }


def _case(file, description, *, load_status, export, closed, solids, shell_closed, faces,
          volume, volume_abs_tol, volume_rel_tol, generate=False, time_budget_s=None,
          known_bug=None, export_status=0, assert_mesh=True):
    step = None
    iges = None
    if export:
        # IGESControl_Reader (writer brep mode 0, the OCCT default for an unregistered
        # write.brep.mode) returns loose faces: no shells and no solids. Face count and
        # GProp volume still match. STEP is where shell/solid closure is checked.
        step = {"status": export_status, "solids": solids, "faces": faces, "shell_closed": shell_closed}
        iges = {"status": export_status, "solids": 0, "faces": faces, "shell_closed": False}
    return {
        "file": file,
        "description": description,
        "generate": generate,
        "load_status": load_status,
        "export": export,
        "closed": closed,
        "volume": volume,
        "volume_abs_tol": volume_abs_tol,
        "volume_rel_tol": volume_rel_tol,
        "time_budget_s": time_budget_s,
        "known_bug": known_bug,
        "assert_mesh": assert_mesh,
        "step": step,
        "iges": iges,
    }


def expectations():
    """Per-file conversion expectations. Geometry is filled in from the mesh."""
    ok = dict(load_status=0, export=True, closed=True, shell_closed=True, export_status=0,
              volume_abs_tol=1e-3, volume_rel_tol=1e-4)
    # Curved meshes: float32 quantization plus OCCT face tolerances.
    curved = dict(ok)
    curved.update(volume_abs_tol=1.0, volume_rel_tol=1e-3)
    repaired = dict(ok)
    repaired.update(export_status=4, volume_abs_tol=1.0, volume_rel_tol=1e-3)
    return {
        "01_cube_20mm_binary.stl": _case(
            "01_cube_20mm_binary.stl",
            "20 mm cube, binary STL. Reference millimetre-scale solid.",
            solids=1, faces=12, volume=8000.0, **ok),
        "02_cube_20mm_ascii.stl": _case(
            "02_cube_20mm_ascii.stl",
            "Same 20 mm cube as ASCII STL.",
            solids=1, faces=12, volume=8000.0, **ok),
        "03_cylinder_d20_h40.stl": _case(
            "03_cylinder_d20_h40.stl",
            "Faceted cylinder, diameter 20, height 40, 64 sides (256 triangles). "
            "The seam vertex at angle 2*pi does not match angle 0 after float32 rounding, "
            "so the default weld closes it and reports REPAIRED.",
            solids=1, faces=256, volume=None, **repaired),
        "04_torus.stl": _case(
            "04_torus.stl",
            "Torus R=30, r=8, 96 x 48 (9216 triangles). Float32 seam vertices do not meet exactly; "
            "the default weld closes the solid and reports REPAIRED.",
            solids=1, faces=9216, volume=None, **repaired),
        "05_sphere_r25_medium.stl": _case(
            "05_sphere_r25_medium.stl",
            "UV sphere radius 25, 128 x 64. The south pole is not a single float32 point; "
            "default weld should close it and report REPAIRED.",
            solids=1, faces=None, volume=None, **repaired),
        "06_two_separate_boxes.stl": _case(
            "06_two_separate_boxes.stl",
            "Two disjoint 10 mm cubes. Two closed solids, volume 2000.",
            solids=2, faces=24, volume=2000.0, **ok),
        "07_open_mesh_cube_missing_top.stl": _case(
            "07_open_mesh_cube_missing_top.stl",
            "Open cube. The attached generator drops triangles 2 and 3, which are the +X face "
            "(the filename says missing top; the omitted face is +X). Export must be an open shell, "
            "status NOT_SOLID, and must not invent a solid.",
            load_status=0, export=True, closed=False, solids=0, shell_closed=False, faces=10,
            volume=None, volume_abs_tol=0.0, volume_rel_tol=0.0, export_status=3),
        "08_tiny_cube_0.1mm.stl": _case(
            "08_tiny_cube_0.1mm.stl",
            "0.1 mm cube. Below typical CAD accuracy but a closed solid; weld must not collapse it.",
            solids=1, faces=12, volume=None, **dict(ok, volume_abs_tol=1e-9, volume_rel_tol=1e-4)),
        LARGE_NAME: _case(
            LARGE_NAME,
            "UV sphere radius 50, 450 x 224 (200700 triangles). Generated at test time, not committed. "
            "South-pole weld reports REPAIRED. time_budget_s is a per-export and per-read ceiling.",
            solids=1, faces=None, volume=None, generate=True, time_budget_s=600, **repaired),
        "10_invalid_not_stl.stl": _case(
            "10_invalid_not_stl.stl",
            "Plain text, not an STL. load returns FORMAT and must not crash.",
            load_status=-3, export=False, closed=False, solids=0, shell_closed=False, faces=0,
            volume=None, volume_abs_tol=0.0, volume_rel_tol=0.0),
        "11_inverted_normals.stl": _case(
            "11_inverted_normals.stl",
            "20 mm cube with every triangle wound inward. Export flips it (REPAIRED) and the "
            "solid volume is +8000.",
            solids=1, faces=12, volume=8000.0, **dict(ok, export_status=4)),
        "12_duplicate_vertices.stl": _case(
            "12_duplicate_vertices.stl",
            "20 mm cube subdivided 2x2 per face (48 triangles). Each geometric vertex is stored "
            "again in every adjacent facet; the loader must weld exact duplicates to 26 vertices "
            "(8 corners, 12 edge midpoints, 6 face centres).",
            solids=1, faces=48, volume=8000.0, **ok),
        "13_degenerate_zero_area.stl": _case(
            "13_degenerate_zero_area.stl",
            "20 mm cube plus a repeated-vertex triangle and a collinear zero-area triangle. "
            "Load skips both (PARTIAL). Export drops them and keeps the cube (REPAIRED).",
            solids=1, faces=12, volume=8000.0,
            **dict(ok, load_status=1, export_status=4)),
        "14_large_coordinates_1e5.stl": _case(
            "14_large_coordinates_1e5.stl",
            "20 mm cube translated to (1e5, 1e5, 1e5). Coordinates stay in range; volume stays 8000.",
            solids=1, faces=12, volume=8000.0, **dict(ok, volume_abs_tol=0.05, volume_rel_tol=1e-4)),
        "15_meter_scale_0.02.stl": _case(
            "15_meter_scale_0.02.stl",
            "Cube of side 0.02, i.e. a 20 mm part stored with metre numbers. STL has no units; "
            "the STEP/IGES file is still millimetres, so the solid is 0.02 mm on a side "
            "(volume 8e-6), not rescaled to 20 mm. Compared with 01_cube_20mm_binary.",
            solids=1, faces=12, volume=None, **dict(ok, volume_abs_tol=1e-12, volume_rel_tol=1e-3)),
        "16_thin_wall.stl": _case(
            "16_thin_wall.stl",
            "Closed plate 50 x 30 x 0.25 mm. Wall is far above the automatic weld tolerance, "
            "so the plate must stay 0.25 thick (volume 375).",
            solids=1, faces=12, volume=375.0, **dict(ok, volume_abs_tol=1e-4, volume_rel_tol=1e-4)),
        "17_through_hole.stl": _case(
            "17_through_hole.stl",
            "40 x 30 x 20 block with a 10 x 10 square through-hole along Z. One closed solid "
            "(genus 1, Euler characteristic 0), volume 22000. The annulus is gridded so "
            "edges meet; a coarser annulus is only T-junctions and is not a solid.",
            solids=1, faces=64, volume=22000.0, **ok),
        "18_empty_binary.stl": _case(
            "18_empty_binary.stl",
            "Binary STL header with a triangle count of 0. No facet is a FORMAT error, not EMPTY "
            "(EMPTY is an operation on a handle that has no mesh).",
            load_status=-3, export=False, closed=False, solids=0, shell_closed=False, faces=0,
            volume=None, volume_abs_tol=0.0, volume_rel_tol=0.0),
        "19_truncated_binary.stl": _case(
            "19_truncated_binary.stl",
            "Binary STL declaring 12 triangles but containing 2 complete records plus 17 trailing "
            "bytes. Load keeps the 2 triangles (PARTIAL). The patch is open, so export is NOT_SOLID.",
            load_status=1, export=True, closed=False, solids=0, shell_closed=False, faces=2,
            volume=None, volume_abs_tol=0.0, volume_rel_tol=0.0, export_status=3),
        "20_ascii_crlf_whitespace.stl": _case(
            "20_ascii_crlf_whitespace.stl",
            "20 mm cube as ASCII with CRLF line endings, tabs, and extra blank lines. "
            "The first five bytes are still 'solid'.",
            solids=1, faces=12, volume=8000.0, **ok),
        "21_ascii_leading_whitespace.stl": _case(
            "21_ascii_leading_whitespace.stl",
            "Same CRLF cube with spaces and a CRLF before the solid keyword.",
            load_status=1, export=True, closed=False, solids=0, shell_closed=False, faces=0,
            volume=None, volume_abs_tol=0.0, volume_rel_tol=0.0, export_status=-6,
            assert_mesh=False,
            known_bug="binary_format() treats a file as binary unless the first five bytes are "
                      "'solid' or the length is an exact binary STL. Leading whitespace therefore "
                      "parses the ASCII text as binary records (PARTIAL) and export fails with OCCT."),
    }


def build_meshes():
    cube = qtris(box(20, 20, 20))
    hole = qtris(through_hole())
    thin = qtris(box(50, 30, 0.25))
    meshes = {
        "01_cube_20mm_binary.stl": ("binary", cube, "cube"),
        "02_cube_20mm_ascii.stl": ("ascii", cube, "cube"),
        "03_cylinder_d20_h40.stl": ("binary", qtris(cyl(10, 40, 64)), None),
        "04_torus.stl": ("binary", qtris(torus(30, 8, 96, 48)), None),
        "05_sphere_r25_medium.stl": ("binary", qtris(sphere(25, 128, 64)), None),
        "06_two_separate_boxes.stl": ("binary", qtris(box(10, 10, 10) + box(10, 10, 10, 30, 0, 0)), None),
        "07_open_mesh_cube_missing_top.stl": (
            "binary",
            [tri for index, tri in enumerate(cube) if index not in (2, 3)],
            None,
        ),
        "08_tiny_cube_0.1mm.stl": ("binary", qtris(box(0.1, 0.1, 0.1)), None),
        LARGE_NAME: ("binary", qtris(sphere(50, 450, 224)), None),
        "10_invalid_not_stl.stl": ("text", "this is not an STL file\n", None),
        "11_inverted_normals.stl": ("binary", invert(cube), None),
        "12_duplicate_vertices.stl": ("binary", qtris(subdivided_box(20, 20, 20, 2)), None),
        "13_degenerate_zero_area.stl": (
            "binary",
            cube + [((0, 0, 0), (0, 0, 0), (1, 0, 0)), ((0, 0, 0), (5, 0, 0), (10, 0, 0))],
            None,
        ),
        "14_large_coordinates_1e5.stl": ("binary", qtris(box(20, 20, 20, 1e5, 1e5, 1e5)), None),
        "15_meter_scale_0.02.stl": ("binary", qtris(box(0.02, 0.02, 0.02)), None),
        "16_thin_wall.stl": ("binary", thin, None),
        "17_through_hole.stl": ("binary", hole, None),
        "18_empty_binary.stl": ("empty", None, None),
        "19_truncated_binary.stl": ("truncated", cube[:2], None),
        "20_ascii_crlf_whitespace.stl": ("crlf", cube, "cube"),
        "21_ascii_leading_whitespace.stl": ("leading", cube, "cube"),
    }
    # Fail loud if a hand-built solid is inside-out or the wrong size.
    hole_volume = mesh_volume(hole)
    boundary, nonmanifold, vertices, edges, _records = boundary_edges(hole)
    if abs(hole_volume - 22000.0) > 1e-6 or boundary or nonmanifold or vertices - edges + 64 != 0:
        raise SystemExit(
            f"through-hole is not a closed genus-1 solid: volume={hole_volume} "
            f"boundary={boundary} bad_edges={nonmanifold} V={vertices} E={edges}"
        )
    thin_volume = mesh_volume(thin)
    if abs(thin_volume - 375.0) > 1e-6:
        raise SystemExit(f"thin-wall volume {thin_volume} != 375 (winding error)")
    if abs(mesh_volume(cube) - 8000.0) > 1e-6:
        raise SystemExit(f"cube volume {mesh_volume(cube)} != 8000 (winding error)")
    return meshes


def write_mesh(path: Path, kind, payload, name) -> None:
    if kind == "binary":
        wb(path, payload)
    elif kind == "ascii":
        wa(path, payload, name)
    elif kind == "crlf":
        wa_crlf(path, payload, name)
    elif kind == "leading":
        wa_leading_space(path, payload, name)
    elif kind == "text":
        path.write_text(payload, encoding="ascii", newline="\n")
    elif kind == "empty":
        path.write_bytes(b"stl2stp test".ljust(80, b" ") + struct.pack("<I", 0))
    elif kind == "truncated":
        write_truncated(path, payload, 12, b"\x00" * 17)
    else:
        raise SystemExit(f"unknown kind {kind}")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    digest.update(path.read_bytes())
    return digest.hexdigest()


def manifest_for(meshes, expect, directory: Path):
    cases = []
    for file, spec in expect.items():
        kind, payload, _name = meshes[file]
        path = directory / file
        if not path.is_file():
            raise SystemExit(f"missing {path}")
        case = dict(spec)
        case["sha256"] = sha256(path)
        case["bytes"] = path.stat().st_size
        if kind in ("binary", "ascii", "crlf", "leading", "truncated"):
            stats = loader_model(payload if kind != "truncated" else payload)
            case["triangles"] = stats["triangles"]
            case["vertices"] = stats["vertices"]
            case["records"] = stats["records"] if kind != "truncated" else 2
            case["bbox_min"] = stats["bbox_min"]
            case["bbox_max"] = stats["bbox_max"]
            case["mesh_volume"] = stats["volume"]
            if case["volume"] is None and case["closed"]:
                case["volume"] = abs(stats["volume"])
            if case["step"] is not None and case["step"]["faces"] is None:
                case["step"]["faces"] = stats["triangles"]
                case["iges"]["faces"] = stats["triangles"]
        else:
            case["triangles"] = 0
            case["vertices"] = 0
            case["records"] = 0
            case["bbox_min"] = None
            case["bbox_max"] = None
            case["mesh_volume"] = None
        cases.append(case)
    return {
        "version": 1,
        "units": "STL coordinates are written unchanged. STEP and IGES length unit is millimetres.",
        "cases": cases,
    }


def dump_manifest(path: Path, manifest) -> None:
    path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


def copy_assets(directory: Path, assets: Path) -> None:
    assets.mkdir(parents=True, exist_ok=True)
    for name in ASSET_NAMES:
        target = assets / name
        data = (directory / name).read_bytes()
        if not target.is_file() or target.read_bytes() != data:
            target.write_bytes(data)


def generate(out: Path, large_dir: Path | None, write_manifest: bool, assets: Path | None) -> dict:
    meshes = build_meshes()
    expect = expectations()
    out.mkdir(parents=True, exist_ok=True)
    for file, (kind, payload, name) in meshes.items():
        if file == LARGE_NAME:
            if large_dir is None:
                continue
            large_dir.mkdir(parents=True, exist_ok=True)
            write_mesh(large_dir / file, kind, payload, name)
            continue
        write_mesh(out / file, kind, payload, name)
    # Hash the large file from wherever it was written; geometry still comes from memory.
    manifest_dir = out
    if large_dir is not None:
        # manifest_for reads bytes from directory/file. Stage a view via the out dir only for small files
        # and read the large hash from large_dir by temporarily pointing that one file.
        pass
    staging = out
    if large_dir is not None and large_dir.resolve() != out.resolve():
        # manifest_for needs every file in one directory. Read hashes explicitly instead.
        staging = out
    manifest = manifest_for_split(meshes, expect, out, large_dir)
    if write_manifest:
        dump_manifest(out / "manifest.json", manifest)
    if assets is not None:
        copy_assets(out, assets)
    return manifest


def manifest_for_split(meshes, expect, small_dir: Path, large_dir: Path | None):
    cases = []
    for file, spec in expect.items():
        kind, payload, _name = meshes[file]
        directory = large_dir if file == LARGE_NAME else small_dir
        case = dict(spec)
        if directory is None or not (directory / file).is_file():
            case["sha256"] = None
            case["bytes"] = None
        else:
            case["sha256"] = sha256(directory / file)
            case["bytes"] = (directory / file).stat().st_size
        if kind in ("binary", "ascii", "crlf", "leading", "truncated"):
            stats = loader_model(payload)
            case["triangles"] = stats["triangles"]
            case["vertices"] = stats["vertices"]
            case["records"] = 2 if kind == "truncated" else stats["records"]
            case["bbox_min"] = stats["bbox_min"]
            case["bbox_max"] = stats["bbox_max"]
            case["mesh_volume"] = stats["volume"]
            if case["volume"] is None and case["closed"]:
                case["volume"] = abs(stats["volume"])
            if case["step"] is not None and case["step"]["faces"] is None:
                case["step"]["faces"] = stats["triangles"]
                case["iges"]["faces"] = stats["triangles"]
        else:
            case["triangles"] = 0
            case["vertices"] = 0
            case["records"] = 0
            case["bbox_min"] = None
            case["bbox_max"] = None
            case["mesh_volume"] = None
        cases.append(case)
    return {
        "version": 1,
        "units": "STL coordinates are written unchanged. STEP and IGES length unit is millimetres.",
        "cases": cases,
    }


def check(corpus: Path, large_dir: Path, assets: Path) -> None:
    with tempfile.TemporaryDirectory(prefix="stl2stp-fixtures-") as tmp:
        fresh = Path(tmp)
        manifest = generate(fresh, large_dir, write_manifest=False, assets=None)
        for case in manifest["cases"]:
            name = case["file"]
            if case["generate"]:
                produced = large_dir / name
                if case["sha256"] != sha256(produced):
                    raise SystemExit(f"large file hash changed while writing {produced}")
                continue
            committed = corpus / name
            if not committed.is_file():
                raise SystemExit(f"missing committed fixture {committed}")
            if committed.read_bytes() != (fresh / name).read_bytes():
                raise SystemExit(f"committed {name} does not match gen_fixtures.py; regenerate and commit it")
        committed_manifest = json.loads((corpus / "manifest.json").read_text(encoding="utf-8"))
        by_file = {case["file"]: case for case in committed_manifest["cases"]}
        geometry = ("triangles", "vertices", "records", "bbox_min", "bbox_max", "mesh_volume", "sha256", "bytes", "volume")
        for case in manifest["cases"]:
            if case["generate"]:
                # Volume/triangle expectations are deterministic; sha256 must match the file just written
                # and the committed manifest (so the budget test checks the same mesh).
                previous = by_file.get(case["file"])
                if previous is None:
                    raise SystemExit(f"manifest is missing generated case {case['file']}")
                for key in ("triangles", "vertices", "records", "volume", "sha256"):
                    if previous.get(key) != case.get(key):
                        raise SystemExit(
                            f"manifest {case['file']} {key}={previous.get(key)} but generator produced {case.get(key)}"
                        )
                continue
            previous = by_file[case["file"]]
            for key in geometry:
                if previous.get(key) != case.get(key):
                    raise SystemExit(
                        f"manifest {case['file']} {key}={previous.get(key)} but generator produced {case.get(key)}"
                    )
        for name in ASSET_NAMES:
            asset = assets / name
            if not asset.is_file() or asset.read_bytes() != (corpus / name).read_bytes():
                raise SystemExit(f"app asset {asset} is missing or stale; rerun gen_fixtures.py")
    print(f"fixture corpus matches gen_fixtures.py; large mesh written to {large_dir / LARGE_NAME}")


def main(argv) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, default=HERE, help="directory for committed-size fixtures")
    parser.add_argument("--large-dir", type=Path, default=None, help="directory for the 200k-triangle sphere")
    parser.add_argument("--assets", type=Path, default=ASSETS, help="Flutter asset copies of a few fixtures")
    parser.add_argument("--check", action="store_true", help="verify committed files against this script")
    args = parser.parse_args(argv)
    if args.check:
        if args.large_dir is None:
            parser.error("--check requires --large-dir")
        check(HERE, args.large_dir, args.assets)
        return 0
    generate(args.out, args.large_dir, write_manifest=True, assets=args.assets if args.out.resolve() == HERE else None)
    written = len(list(args.out.glob("*.stl")))
    print(f"wrote {written} STL files and manifest.json to {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
