#!/usr/bin/env python
# Copyright (c) 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import sys
import unittest

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT_DIR)

import ninjalog_uploader


class NinjalogUploaderTest(unittest.TestCase):
    def test_parse_gn_args(self):
        self.assertEqual(ninjalog_uploader.ParseGNArgs(json.dumps([])), {})

        # Extract current configs from GN's output json.
        self.assertEqual(ninjalog_uploader.ParseGNArgs(json.dumps([
            {
                'current': {'value': 'true'},
                'default': {'value': 'false'},
                'name': 'is_component_build'
            },
        ])), {'is_component_build': 'true'})

        self.assertEqual(ninjalog_uploader.ParseGNArgs(json.dumps([
            {
                'current': {'value': 'true'},
                'default': {'value': 'false'},
                'name': 'is_component_build'
            },
            {
                'current': {'value': 'false'},
                'default': {'value': 'false'},
                'name': 'use_goma'
            },
        ])), {'is_component_build': 'true',
              'use_goma': 'false'})

    def test_get_ninjalog(self):
        # No args => default to cwd.
        self.assertEqual(ninjalog_uploader.GetNinjalog(['ninja']),
                         './.ninja_log')

        # Specified by -C case.
        self.assertEqual(
            ninjalog_uploader.GetNinjalog(['ninja', '-C', 'out/Release']),
            'out/Release/.ninja_log')
        self.assertEqual(
            ninjalog_uploader.GetNinjalog(['ninja', '-Cout/Release']),
            'out/Release/.ninja_log')

        # Invalid -C flag case.
        self.assertEqual(ninjalog_uploader.GetNinjalog(['ninja', '-C']),
                         './.ninja_log')

        # Multiple target directories => use the last directory.
        self.assertEqual(ninjalog_uploader.GetNinjalog(
            ['ninja', '-C', 'out/Release', '-C', 'out/Debug']),
            'out/Debug/.ninja_log')


if __name__ == '__main__':
    unittest.main()
