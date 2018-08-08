# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from blinkpy.common.host_mock import MockHost
from blinkpy.common.system.executive import ScriptError
from blinkpy.common.system.executive_mock import MockExecutive
from blinkpy.w3c.wpt_manifest import WPTManifest


class WPTManifestUnitTest(unittest.TestCase):

    def test_ensure_manifest_copies_new_manifest(self):
        host = MockHost()
        manifest_path = '/mock-checkout/third_party/WebKit/LayoutTests/external/wpt/MANIFEST.json'

        self.assertFalse(host.filesystem.exists(manifest_path))
        WPTManifest.ensure_manifest(host)
        self.assertTrue(host.filesystem.exists(manifest_path))

        webkit_base = '/mock-checkout/third_party/WebKit'
        self.assertEqual(
            host.executive.calls,
            [
                [
                    'python',
                    '/mock-checkout/third_party/blink/tools/blinkpy/third_party/wpt/wpt/wpt',
                    'manifest',
                    '--work',
                    '--tests-root',
                    webkit_base + '/LayoutTests/external/wpt',
                ]
            ]
        )

    def test_ensure_manifest_updates_manifest_if_it_exists(self):
        host = MockHost()
        manifest_path = '/mock-checkout/third_party/WebKit/LayoutTests/external/wpt/MANIFEST.json'

        host.filesystem.write_text_file(manifest_path, '{}')
        self.assertTrue(host.filesystem.exists(manifest_path))

        WPTManifest.ensure_manifest(host)
        self.assertTrue(host.filesystem.exists(manifest_path))

        webkit_base = '/mock-checkout/third_party/WebKit'
        self.assertEqual(
            host.executive.calls,
            [
                [
                    'python',
                    '/mock-checkout/third_party/blink/tools/blinkpy/third_party/wpt/wpt/wpt',
                    'manifest',
                    '--work',
                    '--tests-root',
                    webkit_base + '/LayoutTests/external/wpt',
                ]
            ]
        )

    def test_ensure_manifest_raises_exception(self):
        host = MockHost()
        host.executive = MockExecutive(should_throw=True)

        with self.assertRaises(ScriptError):
            WPTManifest.ensure_manifest(host)

    def test_all_url_items_skips_jsshell_tests(self):
        manifest_json = '''
{
    "items": {
        "manual": {},
        "reftest": {},
        "testharness": {
            "test.any.js": [
                ["/test.any.html", {}],
                ["/test.any.js", {"jsshell": true}]
            ]
        }
    }
}
        '''
        manifest = WPTManifest(manifest_json)
        self.assertEqual(manifest.all_url_items(),
                         {u'/test.any.html': [u'/test.any.html', {}]})

    def test_file_path_to_url_paths(self):
        manifest_json = '''
{
    "items": {
        "manual": {},
        "reftest": {},
        "testharness": {
            "test.any.js": [
                ["/test.any.html", {}],
                ["/test.any.js", {"jsshell": true}]
            ]
        }
    }
}
        '''
        manifest = WPTManifest(manifest_json)
        # Leading slashes should be stripped; and jsshell tests shouldn't be
        # included.
        self.assertEqual(manifest.file_path_to_url_paths('test.any.js'),
                         [u'test.any.html'])
