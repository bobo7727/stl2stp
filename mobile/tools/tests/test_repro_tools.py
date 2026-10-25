#!/usr/bin/env python3
"""Boundary tests for the Linux reproduction tools; Python standard library only.

Run: python3 mobile/tools/tests/test_repro_tools.py
Actual core/OCCT builds are exercised separately by tools/verify.sh.
"""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


MOBILE_ROOT = Path(__file__).resolve().parents[2]
BASH = shutil.which('bash')


@unittest.skipUnless(BASH and sys.platform.startswith('linux'), 'requires Linux and bash')
class ReproductionToolsTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='stl2stp tools ')
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.mobile = self.root / 'source with spaces'
        self.cad = self.mobile / 'core/tools/cadcheck'
        shutil.copytree(MOBILE_ROOT / 'core/tools/cadcheck', self.cad,
                        ignore=shutil.ignore_patterns('__pycache__'))
        (self.mobile / 'tools').mkdir()
        for name in ('verify.sh', 'bootstrap-occt.sh'):
            shutil.copyfile(MOBILE_ROOT / 'tools' / name, self.mobile / 'tools' / name)
        self.caller = self.root / 'unrelated working directory'
        self.caller.mkdir()

    def invoke(self, *args):
        return subprocess.run([str(arg) for arg in args], cwd=self.caller,
                              env=dict(os.environ, PYTHONDONTWRITEBYTECODE='1'),
                              stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                              text=True, timeout=30)

    def test_scripts_parse_and_show_help_from_another_directory(self):
        for script in (self.cad / 'run.sh', self.mobile / 'tools/verify.sh',
                       self.mobile / 'tools/bootstrap-occt.sh'):
            with self.subTest(script=script.name):
                syntax = self.invoke(BASH, '-n', script)
                self.assertEqual(syntax.returncode, 0, syntax.stdout)
                help_result = self.invoke(BASH, script, '--help')
                self.assertEqual(help_result.returncode, 0, help_result.stdout)
                self.assertIn('Usage:', help_result.stdout)

    def test_generator_creates_a_new_directory_with_spaces(self):
        output = self.root / 'new fixtures with spaces'
        result = self.invoke(sys.executable, self.cad / 'gen.py', output)
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertEqual({p.name for p in output.glob('*.stl')}, {
            'rot-cube-4.stl', 'rot-cube-10.stl', 'open-cube-4.stl', 'rot-two-bodies.stl'})
        self.assertTrue(all(p.stat().st_size > 84 for p in output.glob('*.stl')))

    def test_missing_cli_fails_before_creating_output(self):
        build = self.root / 'empty build'
        build.mkdir()
        output = self.root / 'unused output'
        result = self.invoke(BASH, self.cad / 'run.sh', '--build-dir', build,
                             '--output-dir', output)
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn('CLI not found', result.stdout)
        self.assertFalse(output.exists())

    def test_export_error_is_preserved_and_old_output_is_kept(self):
        build = self.root / 'build with spaces'
        (build / 'tests').mkdir(parents=True)
        # These are process-boundary test doubles, not CAD implementations. The
        # fixture generator succeeds, but the first export deliberately fails.
        generator = build / 'tests/stl2stp-fixture-generator'
        generator.write_text('#!/bin/sh\nmkdir -p "$1"\n', encoding='utf-8')
        generator.chmod(0o755)
        cli = build / 'stl2stp-cli'
        cli.write_text('#!/bin/sh\nprintf "deliberate conversion failure\\n" >&2\nexit 23\n',
                       encoding='utf-8')
        cli.chmod(0o755)
        output = self.root / 'output with spaces'
        output.mkdir()
        sentinel = output / 'previous-result.txt'
        sentinel.write_text('preserve this result', encoding='utf-8')

        result = self.invoke(BASH, self.cad / 'run.sh', '--build-dir', build,
                             '--output-dir', output, '--check')
        self.assertEqual(result.returncode, 23, result.stdout)
        self.assertIn('deliberate conversion failure', result.stdout)
        self.assertEqual(sentinel.read_text(), 'preserve this result')
        runs = list(output.glob('run.*'))
        self.assertEqual(len(runs), 1)
        self.assertEqual(len(list((runs[0] / 'fx').glob('*.stl'))), 4)
        self.assertFalse(list((runs[0] / 'out').iterdir()))

    def test_checkers_reject_invalid_and_empty_models(self):
        for label, content in (
            ('malformed', 'not STEP'),
            ('empty', 'ISO-10303-21; HEADER; ENDSEC; DATA; ENDSEC; END-ISO-10303-21;'),
        ):
            model = self.root / (label + ' model.step')
            model.write_text(content, encoding='utf-8')
            for checker in ('step_check.py', 'pcurve_check.py'):
                with self.subTest(model=label, checker=checker):
                    result = self.invoke(sys.executable, self.cad / checker, model)
                    self.assertNotEqual(result.returncode, 0, result.stdout)
            report = json.loads((self.caller / 'step_check_report.json').read_text())
            self.assertNotEqual(report[0]['result'], 'PASS')

    def test_checkers_require_input(self):
        for checker in ('step_check.py', 'pcurve_check.py'):
            result = self.invoke(sys.executable, self.cad / checker)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('Usage:', result.stdout)

    def test_mesh_mode_cannot_claim_cad_validation(self):
        result = self.invoke(BASH, self.mobile / 'tools/verify.sh', '--mesh-only', '--cad')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('--cad requires a full OCCT build', result.stdout)

    def test_missing_occt_has_an_actionable_error(self):
        if not all(shutil.which(tool) for tool in ('cmake', 'ctest', 'ninja')):
            self.skipTest('requires the CMake/Ninja toolchain')
        result = self.invoke(BASH, self.mobile / 'tools/verify.sh',
                             '--occt-root', self.root / 'missing OCCT')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('OCCT headers not found', result.stdout)
        self.assertIn('bootstrap-occt.sh', result.stdout)

    def test_bootstrap_rejects_a_corrupt_cached_archive_without_extracting(self):
        if not all(shutil.which(tool) for tool in ('cmake', 'ninja', 'curl', 'tar', 'sha256sum')):
            self.skipTest('requires the bootstrap command-line tools')
        workspace = self.root / 'OCCT workspace with spaces'
        (workspace / 'downloads').mkdir(parents=True)
        (workspace / 'downloads/V8.0.1.tar.gz').write_bytes(b'corrupt download')
        result = self.invoke(BASH, self.mobile / 'tools/bootstrap-occt.sh',
                             '--work-dir', workspace)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('SHA256 mismatch', result.stdout)
        self.assertFalse((workspace / 'source/OCCT-8.0.1').exists())
        self.assertFalse((workspace / 'build').exists())


if __name__ == '__main__':
    unittest.main(verbosity=2)
