# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
import update_reference_build as update_ref_build


# Disable for accessing private API of update_reference_build class.
# pylint: disable=protected-access
class UpdateReferenceBuildUnittest(unittest.TestCase):

  def testInit(self):
    @classmethod
    def EmptyVersions(_):
      return {}

    @classmethod
    def AllOmahaVersion1(_):
      return {'mac': '1', 'linux': '1', 'win': '1'}

    @classmethod
    def AllCurrentVersion1(_):
      return {'Mac64': '1', 'Linux': '1', 'Linux_x64': '1', 'Win': '1'}

    @classmethod
    def MixedOmahaVersion23(_):
      return {'mac': '2', 'linux': '3', 'win': '2'}

    @classmethod
    def MissingOmahaVersion(_):
      return {'mac': '2', 'win': '1'}
    old_stable = update_ref_build.BuildUpdater._OmahaVersionsMap
    old_current = update_ref_build.BuildUpdater._CurrentRefBuildsMap
    try:
      update_ref_build.BuildUpdater._CurrentRefBuildsMap = EmptyVersions
      update_ref_build.BuildUpdater._OmahaVersionsMap = AllOmahaVersion1
      expected_versions = {
          'Mac64': '1',
          'Linux': '1',
          'Linux_x64': '1',
          'Win': '1'
      }
      b = update_ref_build.BuildUpdater()
      self.assertEqual(expected_versions, b._platform_to_version_map)

      update_ref_build.BuildUpdater._OmahaVersionsMap = MissingOmahaVersion
      expected_versions = {'Mac64': '2', 'Win': '1'}
      b = update_ref_build.BuildUpdater()
      self.assertEqual(expected_versions, b._platform_to_version_map)

      update_ref_build.BuildUpdater._CurrentRefBuildsMap = AllCurrentVersion1
      expected_versions = {'Mac64': '2'}
      b = update_ref_build.BuildUpdater()
      self.assertEqual(expected_versions, b._platform_to_version_map)

      update_ref_build.BuildUpdater._OmahaVersionsMap = MixedOmahaVersion23
      expected_versions = {
          'Mac64': '2',
          'Linux': '3',
          'Linux_x64': '3',
          'Win': '2'
      }
      b = update_ref_build.BuildUpdater()
      self.assertEqual(expected_versions, b._platform_to_version_map)
    finally:
      update_ref_build.BuildUpdater._OmahaVersionsMap = old_stable
      update_ref_build.BuildUpdater._CurrentRefBuildsMap = old_current

  def testOmahaVersions(self):
    # This is an example of valid output from the _OmahaReport function.
    # Taken from processing the omaha report on 3/18/15
    lines = [['os', 'channel', 'current_version', 'previous_version',
              'current_reldate', 'previous_reldate', 'branch_base_commit',
              'branch_base_position', 'branch_commit', 'base_webkit_position',
              'true_branch', 'v8_version\n'],
             ['win', 'stable', '41.0.2272.89', '41.0.2272.76', '03/10/15',
              '03/03/15', '827a380cfdb31aa54c8d56e63ce2c3fd8c3ba4d4', '310958',
              'a4d5695040a99b9b2cb196eb5b898383a274376e', '188177', 'master',
              '4.1.0.21\n'],
             ['mac', 'stable', '41.0.2272.89', '41.0.2272.76', '03/10/15',
              '03/03/15', '827a380cfdb31aa54c8d56e63ce2c3fd8c3ba4d4', '310958',
              'a4d5695040a99b9b2cb196eb5b898383a274376e', '188177', 'master',
              '4.1.0.21\n'],
             ['linux', 'stable', '41.0.2272.89', '41.0.2272.76', '03/10/15',
              '03/03/15', '827a380cfdb31aa54c8d56e63ce2c3fd8c3ba4d4', '310958',
              'a4d5695040a99b9b2cb196eb5b898383a274376e', '188177', 'master',
              '4.1.0.21\n'],
             ['cros', 'stable', '41.0.2272.89', '41.0.2272.76', '03/10/15',
              '03/04/15', '827a380cfdb31aa54c8d56e63ce2c3fd8c3ba4d4', '310958',
              'a4d5695040a99b9b2cb196eb5b898383a274376e', '188177', 'master',
              '4.1.0.21\n'],
             ['android', 'stable', '41.0.2272.94', '40.0.2214.109', '03/18/15',
              '02/04/15', '827a380cfdb31aa54c8d56e63ce2c3fd8c3ba4d4', '310958',
              '70c994cb9b14e4c6934654aaa7089b4b2e8f7788', '188177', '2272',
              '4.1.0.21\n'],
             ['ios', 'stable', '41.0.2272.56', '40.0.2214.73', '03/16/15',
              '02/18/15', 'N/A', 'N/A', 'N/A', 'N/A', 'N/A', 'N/A\n']]

    @classmethod
    def GetLines(_):
      return lines
    old_omaha_report = update_ref_build.BuildUpdater._OmahaReport
    update_ref_build.BuildUpdater._OmahaReport = GetLines
    expected_versions = {'win': '41.0.2272.89', 'mac': '41.0.2272.89',
                         'linux': '41.0.2272.89'}
    b = update_ref_build.BuildUpdater()
    try:
      versions = b._OmahaVersionsMap()
      self.assertEqual(expected_versions, versions)
      lines = [['os', 'channel', 'current_version', 'previous_version',
                'current_reldate', 'previous_reldate', 'branch_base_commit',
                'branch_base_position', 'branch_commit', 'base_webkit_position',
                'true_branch', 'v8_version\n'],
               ['win', 'stable', '41.0.2272.89', '41.0.2272.76', '03/10/15',
                '03/03/15', '827a380cfdb31aa54c8d56e63ce2c3fd8c3ba4d4',
                '310958', 'a4d5695040a99b9b2cb196eb5b898383a274376e', '188177',
                'master', '4.1.0.21\n']]
      self.assertRaises(ValueError, b._OmahaVersionsMap)
      lines = ['random', 'list', 'of', 'strings']
      self.assertRaises(ValueError, b._OmahaVersionsMap)
      lines = []
      self.assertRaises(ValueError, b._OmahaVersionsMap)
    finally:
      update_ref_build.BuildUpdater._OmahaReport = old_omaha_report
