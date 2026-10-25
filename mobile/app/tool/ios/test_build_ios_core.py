#!/usr/bin/env python3
"""Exercise iOS build orchestration with mock Apple/CMake tools on Linux.

The real script runs only inside a temporary fixture. Downloads, compilation,
hash checking and removal are mocked; this does not validate Apple binaries.
"""

import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


MOCK_TOOL = r'''
import json, os, pathlib, sys
name = pathlib.Path(sys.argv[0]).name
args = sys.argv[1:]
with open(os.environ['IOS_MOCK_LOG'], 'a') as log:
    log.write(json.dumps([name, args]) + '\n')
if name == 'shasum':
    sys.stdin.read()
if name == 'libtool':
    output_index = args.index('-o') + 1
    for archive in args[output_index + 1:]:
        if not pathlib.Path(archive).is_file():
            sys.exit('archive argument was split or is missing: ' + archive)
    pathlib.Path(args[output_index]).write_bytes(b'fake archive')
'''


@unittest.skipUnless(os.name == "posix" and shutil.which("bash"), "requires POSIX Bash")
class IosBuildScriptTest(unittest.TestCase):
    def test_changed_architectures_and_paths_with_spaces(self):
        source = Path(__file__).resolve().parents[3]
        with tempfile.TemporaryDirectory(prefix="ios build test ") as scratch:
            root = Path(scratch)
            mobile = root / "mobile source"
            for relative in (
                "app/tool/ios/build_ios_core.sh",
                "app/tool/ios/core_symbols.py",
                "app/lib/core/ffi/bindings.dart",
                "core/include/stl2stp/stl2stp.h",
            ):
                target = mobile / relative
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(source / relative, target)
            (mobile / "app/ios/Flutter").mkdir(parents=True)

            work = root / "dependency cache with spaces"
            (work / "OCCT-8.0.1").mkdir(parents=True)
            (work / "V8.0.1.tar.gz").touch()
            for sdk in ("iphoneos", "iphonesimulator"):
                prefix = work / ("occt-" + sdk)
                prefix.mkdir()
                # A stale sentinel must not skip a new configure/build/install.
                (prefix / ".done").touch()
                for library in ("libTKernel.a", "libTK with spaces.a"):
                    (prefix / library).touch()
                build = work / ("build-core-" + sdk)
                build.mkdir()
                (build / "libstl2stp-core.a").touch()

            tools = root / "mock tools"
            tools.mkdir()
            for name in ("cmake", "shasum", "libtool", "lipo", "xcodebuild", "rm"):
                command = tools / name
                command.write_text("#!" + sys.executable + "\n" + MOCK_TOOL, encoding="utf-8")
                command.chmod(0o755)
            log = root / "calls.jsonl"
            env = dict(os.environ, WORK=str(work), JOBS="1", IOS_MOCK_LOG=str(log))
            env["PATH"] = str(tools) + os.pathsep + env["PATH"]
            script = mobile / "app/tool/ios/build_ios_core.sh"
            for archs, minimum in (("arm64", "14.0"), ("arm64;x86_64", "15.0")):
                env.update(SIM_ARCHS=archs, MIN_IOS=minimum)
                run = subprocess.run(["bash", str(script)], env=env, text=True, capture_output=True)
                self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

            calls = [json.loads(line) for line in log.read_text().splitlines()]
            configurations = [args for name, args in calls if name == "cmake" and "-S" in args]
            occt = [args for args in configurations if str(work / "OCCT-8.0.1") in args]
            self.assertEqual(len(occt), 4, "OCCT must configure both SDKs on both runs despite .done")
            self.assertEqual(sum(name == "cmake" and "--install" in args for name, args in calls), 4)
            self.assertIn("-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64", occt[-1])
            self.assertIn("-DCMAKE_OSX_DEPLOYMENT_TARGET=15.0", occt[-1])
            archives = [args for name, args in calls if name == "libtool"]
            self.assertEqual(len(archives), 4)
            for args in archives:
                self.assertEqual(len(args), 6, "each archive must remain one argument")
                self.assertTrue(any(path.endswith("libTK with spaces.a") for path in args))


if __name__ == "__main__":
    unittest.main()
