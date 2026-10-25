#!/usr/bin/env python3
import unittest

from check_ios_binary import missing_exports


class BinaryExportsTests(unittest.TestCase):
    def test_macho_c_prefix_and_unrelated_exports(self):
        output = '_stl2stp_create\n_stl2stp_weld_vertices\n_other_plugin\n'
        self.assertEqual(missing_exports(output, ['stl2stp_create', 'stl2stp_weld_vertices']), [])

    def test_missing_weld_export_is_reported(self):
        self.assertEqual(missing_exports('_stl2stp_create\n',
                                        ['stl2stp_create', 'stl2stp_weld_vertices']),
                         ['stl2stp_weld_vertices'])

    def test_empty_output_cannot_pass(self):
        self.assertEqual(missing_exports('', ['stl2stp_create']), ['stl2stp_create'])


if __name__ == '__main__':
    unittest.main()
