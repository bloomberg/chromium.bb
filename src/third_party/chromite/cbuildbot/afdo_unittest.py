# -*- coding: utf-8 -*-
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Sometimes we poke 'private' AFDO methods, since that's the most direct way to
# test what we're looking to test. That's OK.
#
# pylint: disable=protected-access

"""Unit tests for afdo module."""

from __future__ import print_function

import collections
import datetime
import json
import os
import time

import mock
from chromite.cbuildbot import afdo
from chromite.lib import (cros_build_lib, cros_test_lib, gs, osutils,
                          path_util, portage_util)

MockGsFile = collections.namedtuple('MockGsFile', ['url', 'creation_time'])


def _benchmark_afdo_profile_name(major=0,
                                 minor=0,
                                 build=0,
                                 patch=0,
                                 rev=1,
                                 merged_suffix=False,
                                 compression_suffix=True):
  suffix = '-merged' if merged_suffix else ''
  result = 'chromeos-chrome-amd64-%d.%d.%d.%d_rc-r%d%s' % (major, minor, build,
                                                           patch, rev, suffix)
  result += afdo.AFDO_SUFFIX
  if compression_suffix:
    result += afdo.COMPRESSION_SUFFIX
  return result


class AfdoTest(cros_test_lib.MockTempDirTestCase):
  """Unit test of afdo module."""

  def testEnumerateMostRecentProfilesRaisesOnNoListing(self):
    mock_gs = mock.Mock()
    mock_gs.List = lambda *args, **kwargs: []
    with self.assertRaises(ValueError):
      afdo._EnumerateMostRecentProfiles(mock_gs, [1, 2, 3], 'some_url', None)

  def testEnumerateMostRecentProfilesFindsTheNewestProfiles(self):

    def mock_list(*_args, **_kwargs):
      return [
          MockGsFile(url='gs://foo/1_1', creation_time=None),
          MockGsFile(url='gs://foo/2_2', creation_time=None),
          MockGsFile(url='gs://foo/2_1', creation_time=None),
          MockGsFile(url='gs://foo/1_2', creation_time=None),
          MockGsFile(url='gs://foo/3_1', creation_time=None),
      ]

    mock_gs = mock.Mock()
    mock_gs.List = mock_list

    Version = collections.namedtuple('Version', ['major', 'minor'])

    def parse_name(name):
      major, minor = name.split('_')
      # Note that the version key uses the *negative* minor number. So _1
      # should be considered the newest. This is to be sure that we're ordering
      # using these Version keys, rather than the strings.
      return Version(int(major), -int(minor))

    milestones = (1, 2, 4)
    most_recent = afdo._EnumerateMostRecentProfiles(mock_gs, milestones, '',
                                                    parse_name)
    self.assertDictEqual(most_recent, {
        1: 'gs://foo/1_1',
        2: 'gs://foo/2_1',
    })

  def testParseProfileMatchesUncompressedProfiles(self):
    # Local profiles will be uncompressed, and the profile parser needs to
    # handle that.
    profile_name = 'chromeos-chrome-amd64-76.0.3795.2_rc-r1.afdo'
    profile_name_compressed = profile_name + '.bz2'
    parsed = afdo._ParseBenchmarkProfileName(profile_name)
    parsed_compressed = afdo._ParseBenchmarkProfileName(profile_name_compressed)
    self.assertEqual(parsed, parsed_compressed)

  def testEnumerateBenchmarkProfilesMatchesRealWorldNames(self):
    enumerate_profiles = self.PatchObject(afdo, '_EnumerateMostRecentProfiles')
    afdo._EnumerateMostRecentBenchmarkProfiles(object(), [1])
    enumerate_profiles.assert_called_once()
    parse_profile = enumerate_profiles.call_args_list[0][0][-1]

    parsed = parse_profile('chromeos-chrome-amd64-57.0.2958.0_rc-r1.afdo.bz2')
    self.assertEqual(parsed.major, 57)
    parsed = parse_profile('chromeos-chrome-amd64-58.0.2959.0_rc-r1.afdo.bz2')
    self.assertEqual(parsed.major, 58)

    # ...Note that not all profiles have the _rc.
    no_rc = parse_profile('chromeos-chrome-amd64-58.0.2959.0-r1.afdo.bz2')
    self.assertEqual(no_rc, parsed)

    # ...And we don't like merged profiles.
    merged_profile = parse_profile(
        'chromeos-chrome-amd64-58.0.2959.0-r1-merged.afdo.bz2')
    self.assertIsNone(merged_profile)

    profile_order = [
        'chromeos-chrome-amd64-10.9.9.9_rc-r9.afdo.bz2',
        'chromeos-chrome-amd64-9.10.9.9_rc-r9.afdo.bz2',
        'chromeos-chrome-amd64-9.9.10.9_rc-r9.afdo.bz2',
        'chromeos-chrome-amd64-9.9.9.10_rc-r9.afdo.bz2',
        'chromeos-chrome-amd64-9.9.9.9_rc-r10.afdo.bz2',
        'chromeos-chrome-amd64-9.9.9.9_rc-r9.afdo.bz2',
    ]

    for higher, lower in zip(profile_order, profile_order[1:]):
      self.assertGreater(parse_profile(higher), parse_profile(lower))

  def testEnumerateCWPProfilesMatchesRealWorldNames(self):
    enumerate_profiles = self.PatchObject(afdo, '_EnumerateMostRecentProfiles')
    afdo._EnumerateMostRecentCWPProfiles(object(), [1])
    enumerate_profiles.assert_called_once()
    parse_profile = enumerate_profiles.call_args_list[0][0][-1]

    parsed = parse_profile('R75-3759.4-1555926322.afdo.xz')
    self.assertEqual(parsed.major, 75)
    parsed = parse_profile('R76-3759.4-1555926322.afdo.xz')
    self.assertEqual(parsed.major, 76)

    profile_order = [
        'R10-9.9-9.afdo.xz',
        'R9-10.9-9.afdo.xz',
        'R9-9.10-9.afdo.xz',
        'R9-9.9-10.afdo.xz',
        'R9-9.9-9.afdo.xz',
    ]

    for higher, lower in zip(profile_order, profile_order[1:]):
      self.assertGreater(parse_profile(higher), parse_profile(lower))

  def testGenerateMergePlanMatchesProfilesAppropriately(self):
    milestones = (1, 2, 3, 4)
    gs_ctx = object()

    def mock_enumerate(gs_context, milestones2, glob_url, _parse_profile_name):
      self.assertIs(milestones, milestones2)
      self.assertIs(gs_context, gs_ctx)

      if afdo.GSURL_BASE_CWP in glob_url:
        return {
            1: 'gs://cwp/1',
            2: 'gs://cwp/2',
            4: 'gs://cwp/4',
        }
      assert afdo.GSURL_BASE_BENCH in glob_url
      return {
          1: 'gs://bench/1',
          2: 'gs://bench/2',
          3: 'gs://bench/3',
      }

    self.PatchObject(afdo, '_EnumerateMostRecentProfiles', mock_enumerate)
    skipped, to_merge = afdo.GenerateReleaseProfileMergePlan(gs_ctx, milestones)
    self.assertEqual(skipped, [3, 4])
    self.assertDictEqual(to_merge, {
        1: ('gs://cwp/1', 'gs://bench/1'),
        2: ('gs://cwp/2', 'gs://bench/2'),
    })

  def testExecuteMergePlanWorks(self):
    mock_gs = mock.Mock()
    gs_copy = mock_gs.Copy
    compress_file = self.PatchObject(cros_build_lib, 'CompressFile')
    uncompress_file = self.PatchObject(cros_build_lib, 'UncompressFile')
    merge_afdo_profiles = self.PatchObject(afdo, '_MergeAFDOProfiles')

    # The only way to know for sure that we created a sufficient set of
    # directories is a tryjob. Just make sure there are no side-effects.
    self.PatchObject(osutils, 'SafeMakedirs')

    merge_plan = {
        1: ('gs://cwp/1.afdo.xz', 'gs://bench/1.afdo.bz2'),
    }

    build_root = '/build/root'
    chroot = os.path.join(build_root, 'chroot')
    merged_files = afdo.ExecuteReleaseProfileMergePlan(mock_gs, build_root,
                                                       merge_plan)

    self.assertSetEqual(set(merged_files.keys()), {1})
    merged_output = merged_files[1]

    def assert_call_args(the_mock, call_args):
      self.assertEqual(the_mock.call_count, len(call_args))
      the_mock.assert_has_calls(call_args)

    assert_call_args(gs_copy, [
        mock.call('gs://bench/1.afdo.bz2',
                  chroot + '/tmp/afdo_data_merge/benchmark.afdo.bz2'),
        mock.call('gs://cwp/1.afdo.xz',
                  chroot + '/tmp/afdo_data_merge/cwp.afdo.xz'),
    ])

    assert_call_args(uncompress_file, [
        mock.call(chroot + '/tmp/afdo_data_merge/benchmark.afdo.bz2',
                  chroot + '/tmp/afdo_data_merge/benchmark.afdo'),
        mock.call(chroot + '/tmp/afdo_data_merge/cwp.afdo.xz',
                  chroot + '/tmp/afdo_data_merge/cwp.afdo'),
    ])

    uncompressed_merged_output = os.path.splitext(merged_output)[0]
    uncompressed_chroot_merged_output = uncompressed_merged_output[len(chroot):]
    assert_call_args(merge_afdo_profiles, [
        mock.call([('/tmp/afdo_data_merge/cwp.afdo', 75),
                   ('/tmp/afdo_data_merge/benchmark.afdo', 25)],
                  uncompressed_chroot_merged_output,
                  use_compbinary=True),
    ])

    assert_call_args(compress_file, [
        mock.call(uncompressed_merged_output, merged_output),
    ])

  def testUploadReleaseProfilesUploadsAsExpected(self):
    mock_gs = mock.Mock()
    gs_copy = mock_gs.Copy
    write_file = self.PatchObject(osutils, 'WriteFile')

    global_tmpdir = '/global/tmp'
    self.PatchObject(osutils, 'GetGlobalTempDir', return_value=global_tmpdir)

    merge_plan = {
        1: ('gs://cwp/1.afdo.xz', 'gs://bench/1.afdo.bz2'),
        2: ('gs://cwp/2.afdo.xz', 'gs://bench/2.afdo.bz2'),
    }

    merge_results = {
        1: '/tmp/foo.afdo.bz2',
        2: '/tmp/bar.afdo.bz2',
    }

    run_id = '1234'
    afdo.UploadReleaseProfiles(mock_gs, run_id, merge_plan, merge_results)

    write_file.assert_called_once()
    meta_file_local_location, meta_file_data = write_file.call_args_list[0][0]
    self.assertEqual(meta_file_data, json.dumps(merge_plan))

    self.assertTrue(meta_file_local_location.startswith(global_tmpdir))

    def expected_upload_location(profile_version):
      return os.path.join(afdo.GSURL_BASE_RELEASE, run_id,
                          'profiles/m%d.afdo.bz2' % profile_version)

    expected_copy_calls = [
        mock.call(
            merge_results[1],
            expected_upload_location(1),
            acl='public-read',
            version=0),
        mock.call(
            merge_results[2],
            expected_upload_location(2),
            acl='public-read',
            version=0),
        mock.call(
            meta_file_local_location,
            os.path.join(afdo.GSURL_BASE_RELEASE, run_id, 'meta.json'),
            acl='public-read',
            version=0),
    ]

    gs_copy.assert_has_calls(expected_copy_calls)
    self.assertEqual(gs_copy.call_count, len(expected_copy_calls))

  def runCreateAndUploadMergedAFDOProfileOnce(self, upload_ok=True, **kwargs):
    if 'unmerged_name' not in kwargs:
      # Match everything.
      kwargs['unmerged_name'] = _benchmark_afdo_profile_name(major=9999)

    Mocks = collections.namedtuple('Mocks', [
        'gs_context',
        'run_command',
        'uncompress_file',
        'compress_file',
        'upload',
        'remove_indirect_call_targets',
    ])

    def MockList(*_args, **_kwargs):
      files = [
          _benchmark_afdo_profile_name(major=10, build=9),
          _benchmark_afdo_profile_name(major=10, build=10),
          _benchmark_afdo_profile_name(major=10, build=10, merged_suffix=True),
          _benchmark_afdo_profile_name(major=10, build=11),
          _benchmark_afdo_profile_name(major=10, build=12),
          _benchmark_afdo_profile_name(major=10, build=13),
          _benchmark_afdo_profile_name(major=10, build=13, merged_suffix=True),
          _benchmark_afdo_profile_name(major=10, build=13, patch=1),
          _benchmark_afdo_profile_name(major=10, build=13, patch=2),
          _benchmark_afdo_profile_name(
              major=10, build=13, patch=2, merged_suffix=True),
          _benchmark_afdo_profile_name(major=11, build=14),
          _benchmark_afdo_profile_name(major=11, build=14, merged_suffix=True),
          _benchmark_afdo_profile_name(major=11, build=15),
      ]

      results = []
      for i, name in enumerate(files):
        url = os.path.join(afdo.GSURL_BASE_BENCH, name)
        now = datetime.datetime(year=1990, month=1, day=1 + i)
        results.append(MockGsFile(url=url, creation_time=now))
      return results

    mock_gs = mock.Mock()
    mock_gs.List = MockList
    run_command = self.PatchObject(cros_build_lib, 'run')
    uncompress_file = self.PatchObject(cros_build_lib, 'UncompressFile')
    compress_file = self.PatchObject(cros_build_lib, 'CompressFile')
    upload = self.PatchObject(afdo, 'GSUploadIfNotPresent')
    remove_indirect_call_targets = self.PatchObject(
        afdo, '_RemoveIndirectCallTargetsFromProfile')
    upload.return_value = upload_ok
    merged_name, uploaded = afdo.CreateAndUploadMergedAFDOProfile(
        mock_gs, '/buildroot', **kwargs)
    return merged_name, uploaded, Mocks(
        gs_context=mock_gs,
        run_command=run_command,
        uncompress_file=uncompress_file,
        compress_file=compress_file,
        upload=upload,
        remove_indirect_call_targets=remove_indirect_call_targets)

  def testCreateAndUploadMergedAFDOProfileMergesBranchProfiles(self):
    unmerged_name = _benchmark_afdo_profile_name(major=10, build=13, patch=99)

    _, uploaded, mocks = \
        self.runCreateAndUploadMergedAFDOProfileOnce(
            recent_to_merge=5,
            unmerged_name=unmerged_name)
    self.assertTrue(uploaded)

    def _afdo_name(major, build, patch=0, merged_suffix=False):
      return _benchmark_afdo_profile_name(
          major=major,
          build=build,
          patch=patch,
          merged_suffix=merged_suffix,
          compression_suffix=False)

    expected_unordered_args = [
        '-output=/tmp/raw-' +
        _afdo_name(major=10, build=13, patch=2, merged_suffix=True),
        '-weighted-input=1,/tmp/' + _afdo_name(major=10, build=11),
        '-weighted-input=1,/tmp/' + _afdo_name(major=10, build=12),
        '-weighted-input=1,/tmp/' + _afdo_name(major=10, build=13),
        '-weighted-input=1,/tmp/' + _afdo_name(major=10, build=13, patch=1),
        '-weighted-input=1,/tmp/' + _afdo_name(major=10, build=13, patch=2),
    ]

    # Note that these should all be in-chroot names.
    expected_ordered_args = ['llvm-profdata', 'merge', '-sample']

    args = mocks.run_command.call_args[0][0]
    ordered_args = args[:len(expected_ordered_args)]
    self.assertEqual(ordered_args, expected_ordered_args)

    unordered_args = args[len(expected_ordered_args):]
    self.assertCountEqual(unordered_args, expected_unordered_args)
    self.assertEqual(mocks.gs_context.Copy.call_count, 5)

  def testCreateAndUploadMergedAFDOProfileRemovesIndirectCallTargets(self):
    unmerged_name = _benchmark_afdo_profile_name(major=10, build=13, patch=99)

    merged_name, uploaded, mocks = \
        self.runCreateAndUploadMergedAFDOProfileOnce(
            recent_to_merge=2,
            unmerged_name=unmerged_name)
    self.assertTrue(uploaded)

    def _afdo_name(major, build, patch=0, merged_suffix=False):
      return _benchmark_afdo_profile_name(
          major=major,
          build=build,
          patch=patch,
          merged_suffix=merged_suffix,
          compression_suffix=False)

    merge_output_name = 'raw-' + _afdo_name(
        major=10, build=13, patch=2, merged_suffix=True)
    self.assertNotEqual(merged_name, merge_output_name)

    expected_unordered_args = [
        '-output=/tmp/' + merge_output_name,
        '-weighted-input=1,/tmp/' + _afdo_name(major=10, build=13, patch=1),
        '-weighted-input=1,/tmp/' + _afdo_name(major=10, build=13, patch=2),
    ]

    # Note that these should all be in-chroot names.
    expected_ordered_args = ['llvm-profdata', 'merge', '-sample']
    args = mocks.run_command.call_args[0][0]
    ordered_args = args[:len(expected_ordered_args)]
    self.assertEqual(ordered_args, expected_ordered_args)

    unordered_args = args[len(expected_ordered_args):]
    self.assertCountEqual(unordered_args, expected_unordered_args)

    mocks.remove_indirect_call_targets.assert_called_once_with(
        '/tmp/' + merge_output_name, '/tmp/' + merged_name)

  def testCreateAndUploadMergedAFDOProfileWorksInTheHappyCase(self):
    merged_name, uploaded, mocks = \
        self.runCreateAndUploadMergedAFDOProfileOnce(recent_to_merge=5)

    self.assertTrue(uploaded)
    # Note that we always return the *basename*
    self.assertEqual(
        merged_name,
        _benchmark_afdo_profile_name(
            major=11, build=15, merged_suffix=True, compression_suffix=False))

    self.assertTrue(uploaded)
    mocks.run_command.assert_called_once()

    # Note that these should all be in-chroot names.
    expected_ordered_args = ['llvm-profdata', 'merge', '-sample']

    def _afdo_name(major, build, patch=0, merged_suffix=False):
      return _benchmark_afdo_profile_name(
          major=major,
          build=build,
          patch=patch,
          merged_suffix=merged_suffix,
          compression_suffix=False)

    input_afdo_names = [
        _afdo_name(major=10, build=13),
        _afdo_name(major=10, build=13, patch=1),
        _afdo_name(major=10, build=13, patch=2),
        _afdo_name(major=11, build=14),
        _afdo_name(major=11, build=15),
    ]

    output_afdo_name = _afdo_name(major=11, build=15, merged_suffix=True)
    expected_unordered_args = ['-output=/tmp/raw-' + output_afdo_name]
    expected_unordered_args += [
        '-weighted-input=1,/tmp/' + n for n in input_afdo_names
    ]

    args = mocks.run_command.call_args[0][0]
    ordered_args = args[:len(expected_ordered_args)]
    self.assertEqual(ordered_args, expected_ordered_args)

    unordered_args = args[len(expected_ordered_args):]
    self.assertCountEqual(unordered_args, expected_unordered_args)
    self.assertEqual(mocks.gs_context.Copy.call_count, 5)

    self.assertEqual(mocks.uncompress_file.call_count, 5)

    def call_for(name):
      basis = '/buildroot/chroot/tmp/' + name
      return mock.call(basis + afdo.COMPRESSION_SUFFIX, basis)

    mocks.uncompress_file.assert_has_calls(
        any_order=True, calls=[call_for(n) for n in input_afdo_names])

    compressed_output_afdo_name = output_afdo_name + afdo.COMPRESSION_SUFFIX
    compressed_target = '/buildroot/chroot/tmp/' + compressed_output_afdo_name
    mocks.compress_file.assert_called_once()
    args = mocks.compress_file.call_args[0]
    self.assertEqual(args, (
        compressed_target[:-len(afdo.COMPRESSION_SUFFIX)],
        compressed_target,
    ))

    mocks.upload.assert_called_once()
    args = mocks.upload.call_args[0]

    self.assertEqual(args, (
        mocks.gs_context,
        compressed_target,
        '%s/%s' % (afdo.GSURL_BASE_BENCH, compressed_output_afdo_name),
    ))

  def testCreateAndUploadMergedAFDOProfileSucceedsIfUploadFails(self):
    merged_name, uploaded, _ = \
        self.runCreateAndUploadMergedAFDOProfileOnce(upload_ok=False)
    self.assertIsNotNone(merged_name)
    self.assertFalse(uploaded)

  def testMergeIsOKIfWeFindFewerProfilesThanWeWant(self):
    merged_name, uploaded, mocks = \
        self.runCreateAndUploadMergedAFDOProfileOnce(recent_to_merge=1000,
                                                     max_age_days=1000)
    self.assertTrue(uploaded)
    self.assertIsNotNone(merged_name)
    self.assertEqual(mocks.gs_context.Copy.call_count, 9)

  def testNoProfileIsGeneratedIfNoFilesBeforeMergedNameExist(self):
    merged_name, uploaded, _ = \
        self.runCreateAndUploadMergedAFDOProfileOnce(
            unmerged_name=_benchmark_afdo_profile_name())
    self.assertIsNone(merged_name)
    self.assertFalse(uploaded)

    merged_name, uploaded, _ = \
        self.runCreateAndUploadMergedAFDOProfileOnce(
            unmerged_name=_benchmark_afdo_profile_name(major=10, build=8))
    self.assertIsNone(merged_name)
    self.assertFalse(uploaded)

    merged_name, uploaded, _ = \
        self.runCreateAndUploadMergedAFDOProfileOnce(
            unmerged_name=_benchmark_afdo_profile_name(major=10, build=9))
    self.assertIsNone(merged_name)
    self.assertFalse(uploaded)

    merged_name, uploaded, _ = \
        self.runCreateAndUploadMergedAFDOProfileOnce(
            unmerged_name=_benchmark_afdo_profile_name(major=10, build=10))
    self.assertIsNotNone(merged_name)
    self.assertTrue(uploaded)

  def testNoFilesAfterUnmergedNameAreIncluded(self):
    max_name = _benchmark_afdo_profile_name(major=10, build=11)
    merged_name, uploaded, mocks = \
        self.runCreateAndUploadMergedAFDOProfileOnce(unmerged_name=max_name)

    self.assertEqual(
        _benchmark_afdo_profile_name(
            major=10, build=11, merged_suffix=True, compression_suffix=False),
        merged_name)
    self.assertTrue(uploaded)

    def _afdo_name(major, build, merged_suffix=False):
      return _benchmark_afdo_profile_name(
          major=major,
          build=build,
          merged_suffix=merged_suffix,
          compression_suffix=False)

    # Note that these should all be in-chroot names.
    expected_ordered_args = ['llvm-profdata', 'merge', '-sample']
    expected_unordered_args = [
        '-output=/tmp/raw-' +
        _afdo_name(major=10, build=11, merged_suffix=True),
        '-weighted-input=1,/tmp/' + _afdo_name(major=10, build=9),
        '-weighted-input=1,/tmp/' + _afdo_name(major=10, build=10),
        '-weighted-input=1,/tmp/' + _afdo_name(major=10, build=11),
    ]

    args = mocks.run_command.call_args[0][0]
    ordered_args = args[:len(expected_ordered_args)]
    self.assertEqual(ordered_args, expected_ordered_args)

    unordered_args = args[len(expected_ordered_args):]
    self.assertCountEqual(unordered_args, expected_unordered_args)

    self.assertEqual(mocks.gs_context.Copy.call_count, 3)
    self.assertEqual(mocks.uncompress_file.call_count, 3)

  def testMergeDoesntHappenIfNoProfilesAreMerged(self):
    runs = [
        self.runCreateAndUploadMergedAFDOProfileOnce(recent_to_merge=1),
        self.runCreateAndUploadMergedAFDOProfileOnce(max_age_days=0),
    ]

    for merged_name, uploaded, mocks in runs:
      self.assertIsNone(merged_name)
      self.assertFalse(uploaded)
      mocks.gs_context.Copy.assert_not_called()
      mocks.run_command.assert_not_called()
      mocks.uncompress_file.assert_not_called()
      mocks.compress_file.assert_not_called()
      mocks.upload.assert_not_called()

  def testRemoveIndirectCallTargetsActuallyAppearsToWork(self):
    run_command = self.PatchObject(cros_build_lib, 'run')
    path_exists = self.PatchObject(os.path, 'exists', return_value=False)

    input_path = '/input/path'
    input_path_txt = input_path + '.txt'
    output_path = '/output/path'
    output_path_txt = output_path + '.txt'
    afdo._RemoveIndirectCallTargetsFromProfile(input_path, output_path)

    self.assertEqual(run_command.call_count, 3)
    merge_to_text, removal, merge_to_bin = run_command.call_args_list

    path_exists.insert_called_with(os.path.join('/chroot', input_path_txt))
    self.assertEqual(
        merge_to_text,
        mock.call(
            [
                'llvm-profdata',
                'merge',
                '-sample',
                '-output=%s' % input_path_txt,
                '-text',
                input_path,
            ],
            enter_chroot=True,
            print_cmd=True,
        ))

    # Probably no value in checking for the actual script name.
    script_name = removal[0][0][0]
    removal.assert_equal(
        removal,
        mock.call(
            [
                script_name,
                '--input=%s' % input_path_txt,
                '--output=%s' % output_path_txt,
            ],
            enter_chroot=True,
            print_cmd=True,
        ))

    self.assertEqual(
        merge_to_bin,
        mock.call(
            [
                'llvm-profdata',
                'merge',
                '-sample',
                '-output=' + output_path,
                output_path_txt,
            ],
            enter_chroot=True,
            print_cmd=True,
        ))

  def testFindLatestProfile(self):
    versions = [[1, 0, 0, 0], [1, 2, 3, 4], [2, 2, 2, 2]]
    self.assertEqual(afdo.FindLatestProfile([0, 0, 0, 0], versions), None)
    self.assertEqual(
        afdo.FindLatestProfile([1, 0, 0, 0], versions), [1, 0, 0, 0])
    self.assertEqual(
        afdo.FindLatestProfile([1, 2, 0, 0], versions), [1, 0, 0, 0])
    self.assertEqual(
        afdo.FindLatestProfile([9, 9, 9, 9], versions), [2, 2, 2, 2])

  def testPatchKernelEbuild(self):
    before = [
        'The following line contains the version:',
        'AFDO_PROFILE_VERSION="R63-9901.21-1506581597"', 'It should be changed.'
    ]
    after = [
        'The following line contains the version:',
        'AFDO_PROFILE_VERSION="R12-3456.78-9876543210"', 'It should be changed.'
    ]
    tf = os.path.join(self.tempdir, 'test.ebuild')
    osutils.WriteFile(tf, '\n'.join(before))
    afdo.PatchKernelEbuild(tf, [12, 3456, 78, 9876543210])
    x = osutils.ReadFile(tf).splitlines()
    self.assertEqual(after, x)

  def testGetAvailableKernelProfiles(self):

    def MockGsList(path):
      unused = {
          'content_length': None,
          'creation_time': None,
          'generation': None,
          'metageneration': None
      }
      path = path.replace('*', '%s')
      return [
          gs.GSListResult(
              url=(path % ('4.4', 'R63-9901.21-1506581597')), **unused),
          gs.GSListResult(
              url=(path % ('3.8', 'R61-9765.70-1506575230')), **unused),
      ]

    self.PatchObject(gs.GSContext, 'List',
                     lambda _, path, **kwargs: MockGsList(path))
    profiles = afdo.GetAvailableKernelProfiles()
    self.assertIn([63, 9901, 21, 1506581597], profiles['4.4'])
    self.assertIn([61, 9765, 70, 1506575230], profiles['3.8'])

  def testFindKernelEbuilds(self):
    ebuilds = [(os.path.basename(ebuild[0]), ebuild[1])
               for ebuild in afdo.FindKernelEbuilds()]
    self.assertIn(('chromeos-kernel-4_4-9999.ebuild', '4.4'), ebuilds)
    self.assertIn(('chromeos-kernel-3_8-9999.ebuild', '3.8'), ebuilds)

  def testProfileAge(self):
    self.assertEqual(0, afdo.ProfileAge([0, 0, 0, int(time.time())]))
    self.assertEqual(1, afdo.ProfileAge([0, 0, 0, int(time.time() - 86400)]))

  def testGetCWPProfile(self):
    profiles = [
        'R62-3202.43-320243.afdo.xz', 'R63-3223.0-233200.afdo.xz',
        'R63-3239.20-323920.afdo.xz', 'R63-3239.42-323942.afdo.xz',
        'R63-3239.50-323950.afdo.xz', 'R63-3239.50-323999.afdo.xz',
        'R64-3280.5-328005.afdo.xz', 'R64-3282.41-328241.afdo.xz',
        'R65-3299.0-329900.afdo.xz'
    ]

    def MockGsList(path):
      unused = {
          'content_length': None,
          'creation_time': None,
          'generation': None,
          'metageneration': None
      }
      return [
          gs.GSListResult(url=os.path.join(path, f), **unused) for f in profiles
      ]

    self.PatchObject(gs.GSContext, 'List',
                     lambda _, path, **kwargs: MockGsList(path))

    def _test(version, idx):
      unused = {
          'pv': None,
          'package': None,
          'version_no_rev': None,
          'rev': None,
          'category': None,
          'cpv': None,
          'cp': None,
          'cpf': None
      }
      cpv = portage_util.CPV(version=version, **unused)
      profile = afdo.GetCWPProfile(cpv, 'silvermont', 'unused', gs.GSContext())
      # Expect the most recent profile on the same branch.
      self.assertEqual(profile, profiles[idx][:-3])

    _test('66.0.3300.0_rc-r1', 8)
    _test('65.0.3283.0_rc-r1', 7)
    _test('65.0.3283.1_rc-r1', 7)
    _test('64.0.3282.42_rc-r1', 7)
    _test('64.0.3282.40_rc-r1', 6)
    _test('63.0.3239.30_rc-r1', 2)
    _test('63.0.3239.42_rc-r0', 2)
    _test('63.0.3239.10_rc-r1', 1)

  def testCWPProfileToVersionTuple(self):
    self.assertEqual(
        afdo.CWPProfileToVersionTuple('gs://chromeos-prebuilt/afdo-job/cwp/'
                                      'chrome/R66-3325.65-1519321598.afdo.xz'),
        [66, 3325, 65, 1519321598])
    self.assertEqual(
        afdo.CWPProfileToVersionTuple('R66-3325.65-1519321598.afdo.xz'),
        [66, 3325, 65, 1519321598])

  def testPatchChromeEbuildAFDOFile(self):
    before = [
        'The following line contains the version:',
        'AFDO_FILE["benchmark"]="chromeos-chrome-amd64-67.0.3379.0_rc-r1.afdo"',
        'AFDO_FILE["silvermont"]="R67-3359.31-1522059092.afdo"',
        'AFDO_FILE["airmont"]="airmont_before.afdo"',
        'AFDO_FILE["broadwell"]="broadwell_before.afdo"',
        'It should be changed.'
    ]
    after = [
        'The following line contains the version:',
        'AFDO_FILE["benchmark"]="chromeos-chrome-amd64-67.0.3388.0_rc-r1.afdo"',
        'AFDO_FILE["silvermont"]="R67-3360.42-153456789.afdo"',
        'AFDO_FILE["airmont"]="airmont_after.afdo"',
        'AFDO_FILE["broadwell"]="broadwell_after.afdo"', 'It should be changed.'
    ]

    self.PatchObject(path_util, 'FromChrootPath', lambda x: x)

    tf = os.path.join(self.tempdir, 'test.ebuild')
    osutils.WriteFile(tf, '\n'.join(before))
    afdo.PatchChromeEbuildAFDOFile(
        tf, {
            'benchmark': 'chromeos-chrome-amd64-67.0.3388.0_rc-r1.afdo',
            'broadwell': 'broadwell_after.afdo',
            'airmont': 'airmont_after.afdo',
            'silvermont': 'R67-3360.42-153456789.afdo'
        })
    x = osutils.ReadFile(tf).splitlines()
    self.assertEqual(after, x)
