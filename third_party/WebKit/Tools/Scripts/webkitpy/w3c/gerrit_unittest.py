# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from webkitpy.common.host import Host
from webkitpy.common.path_finder import PathFinder
from webkitpy.w3c.gerrit import GerritCL
from webkitpy.w3c.gerrit_mock import MockGerritAPI


class TestExporterTest(unittest.TestCase):

    def test_filter_transform_patch(self):
        host = Host()
        finder = PathFinder(host.filesystem)
        resources_path = finder.path_from_tools_scripts('webkitpy', 'w3c', 'resources')
        sample_patch = host.filesystem.read_text_file(host.filesystem.join(resources_path, 'sample.patch'))
        expected_patch = host.filesystem.read_text_file(host.filesystem.join(resources_path, 'expected.patch'))

        cl = GerritCL({'change_id': 1}, MockGerritAPI(None, None, None))
        actual_patch = cl.filter_transform_patch(sample_patch)
        self.assertEqual(actual_patch, expected_patch)

    def test_filter_transform_patch_removes_baselines(self):
        host = Host()
        finder = PathFinder(host.filesystem)
        resources_path = finder.path_from_tools_scripts('webkitpy', 'w3c', 'resources')
        sample_patch = host.filesystem.read_text_file(host.filesystem.join(resources_path, 'sample2.patch'))
        expected_patch = host.filesystem.read_text_file(host.filesystem.join(resources_path, 'expected2.patch'))

        cl = GerritCL({'change_id': 1}, MockGerritAPI(None, None, None))
        actual_patch = cl.filter_transform_patch(sample_patch)
        self.assertEqual(actual_patch, expected_patch)

    def test_strip_commit_positions(self):
        commit_with_footers = ('Test commit\nChange-Id: foobar\n'
                               'Cr-Original-Commit-Position: refs/heads/master@{#10}\n'
                               'Cr-Commit-Position: refs/heads/master@{#10}')
        self.assertEqual(GerritCL.strip_commit_positions(commit_with_footers), 'Test commit\nChange-Id: foobar')
