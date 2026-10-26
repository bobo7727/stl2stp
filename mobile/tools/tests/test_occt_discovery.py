#!/usr/bin/env python3
"""Configure the real core with iOS-style SDK-only dependency discovery.

No Apple SDK or OCCT build is required. The staged dependency fixture is used
only for CMake configuration; its empty archives are never compiled or linked.
"""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


CORE = Path(__file__).resolve().parents[2] / 'core'
TOOLKITS = ('TKDESTEP', 'TKDEIGES', 'TKXSBase', 'TKBool', 'TKBO', 'TKPrim',
            'TKShHealing', 'TKTopAlgo', 'TKGeomAlgo', 'TKBRep', 'TKGeomBase',
            'TKG3d', 'TKG2d', 'TKMath', 'TKernel', 'TKDE')


@unittest.skipUnless(os.name == 'posix' and all(shutil.which(x) for x in ('cmake', 'ninja', 'cc', 'c++')),
                     'requires the POSIX CMake/Ninja C/C++ toolchain')
class OcctDiscoveryTests(unittest.TestCase):
    def configure(self, header_layout):
        scratch = tempfile.TemporaryDirectory(prefix='OCCT SDK discovery ')
        self.addCleanup(scratch.cleanup)
        root = Path(scratch.name)
        prefix = root / 'staged target dependency'
        library_dir = prefix / 'lib'
        library_dir.mkdir(parents=True)
        for name in TOOLKITS:
            (library_dir / f'lib{name}.a').write_bytes(b'!<arch>\n')
        if header_layout is not None:
            include = prefix / header_layout
            include.mkdir(parents=True)
            (include / 'Standard_Version.hxx').write_text('// discovery fixture\n')
        config_dir = library_dir / 'cmake/opencascade'
        config_dir.mkdir(parents=True)
        (config_dir / 'OpenCASCADEConfig.cmake').write_text(
            'set(OpenCASCADE_FOUND TRUE)\n'
            'set(STL2STP_DISCOVERY_PACKAGE_LOADED TRUE CACHE BOOL "regression fixture" FORCE)\n')
        sdk = root / 'empty Apple SDK root'
        sdk.mkdir()
        build = root / 'configure output'
        result = subprocess.run([
            'cmake', '-S', str(CORE), '-B', str(build), '-G', 'Ninja',
            '-DCMAKE_C_COMPILER=' + shutil.which('cc'),
            '-DCMAKE_CXX_COMPILER=' + shutil.which('c++'),
            '-DOCCT_ROOT=' + str(prefix),
            '-DCMAKE_FIND_ROOT_PATH=' + str(sdk),
            '-DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY',
            '-DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY',
            '-DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ONLY',
            '-DBUILD_TESTING=OFF', '-DSTL2STP_BUILD_CLI=OFF',
        ], text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=60)
        return result, build, prefix

    def test_external_target_prefix_is_found_under_sdk_only_modes(self):
        for layout in ('include/opencascade', 'inc', 'include'):
            with self.subTest(header_layout=layout):
                result, build, prefix = self.configure(layout)
                self.assertEqual(result.returncode, 0, result.stdout)
                cache = (build / 'CMakeCache.txt').read_text()
                self.assertIn('OCCT_INCLUDE_DIR:PATH=' + str(prefix / layout), cache)
                self.assertIn('OCCT_LIBRARY_DIR:PATH=' + str(prefix / 'lib'), cache)
                self.assertIn('_occt_TKernel:FILEPATH=' + str(prefix / 'lib/libTKernel.a'), cache)
                self.assertIn('STL2STP_DISCOVERY_PACKAGE_LOADED:BOOL=TRUE', cache)

    def test_missing_headers_do_not_fall_back_to_a_host_install(self):
        result, _, _ = self.configure(None)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('OCCT headers not found', result.stdout)


if __name__ == '__main__':
    unittest.main()
