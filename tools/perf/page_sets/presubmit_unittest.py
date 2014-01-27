# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import os
import sys
import unittest

PERF_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(os.path.dirname(PERF_ROOT), 'telemetry'))
from telemetry.unittest import system_stub

sys.path.insert(0, PERF_ROOT)
from page_sets import PRESUBMIT


class AffectedFileStub(object):
  def __init__(self, absolute_local_path):
    self._absolute_local_path = absolute_local_path

  def AbsoluteLocalPath(self):
    return self._absolute_local_path


class InputAPIStub(object):
  def __init__(self, paths, deleted_paths=None):
    self._paths = paths
    if deleted_paths:
      self._deleted_paths = deleted_paths
    else:
      self._deleted_paths = []

  def AffectedFiles(self, include_deletes=True):
    affected_files = [AffectedFileStub(path) for path in self._paths]
    if include_deletes:
      affected_files += [AffectedFileStub(path) for path in self._deleted_paths]
    return affected_files

  def AbsoluteLocalPaths(self):
    return [af.AbsoluteLocalPath() for af in self.AffectedFiles()]

  def PresubmitLocalPath(self):
    return PRESUBMIT.__file__


class OutputAPIStub(object):
  class PresubmitError(Exception):
    pass

  class PresubmitNotifyResult(Exception):
    pass


PRESUBMIT.LoadSupport(InputAPIStub([]))   # do this to support monkey patching

class PresubmitTest(unittest.TestCase):
  def setUp(self):
    success_file_hash = 'da39a3ee5e6b4b0d3255bfef95601890afd80709'

    self._stubs = system_stub.Override(
        PRESUBMIT, ['cloud_storage', 'open', 'os', 'raw_input'])
    self._stubs.raw_input.input = 'public'
    # Files in Cloud Storage.
    self._stubs.cloud_storage.remote_paths = [
        'skip'.zfill(40),
    ]
    # Local data files and their hashes.
    self._stubs.cloud_storage.local_file_hashes = {
        '/path/to/skip.wpr': 'skip'.zfill(40),
        '/path/to/success.wpr': success_file_hash,
        '/path/to/wrong_hash.wpr': success_file_hash,
    }
    # Local data files.
    self._stubs.os.path.files = (
        self._stubs.cloud_storage.local_file_hashes.keys())
    # Local hash files and their contents.
    self._stubs.open.files = {
        '/path/to/invalid_hash.wpr.sha1': 'invalid_hash',
        '/path/to/missing.wpr.sha1': 'missing'.zfill(40),
        '/path/to/success.wpr.sha1': success_file_hash,
        '/path/to/skip.wpr.sha1': 'skip'.zfill(40),
        '/path/to/wrong_hash.wpr.sha1': 'wronghash'.zfill(40),
    }

  def tearDown(self):
    self._stubs.Restore()

  def assertResultCount(self, results, expected_errors, expected_notifications):
    counts = collections.defaultdict(int)
    for result in results:
      counts[type(result)] += 1
    actual_errors = counts[OutputAPIStub.PresubmitError]
    actual_notifications = counts[OutputAPIStub.PresubmitNotifyResult]
    self.assertEqual(expected_errors, actual_errors,
        msg='Expected %d errors, but got %d. Results: %s' %
        (expected_errors, actual_errors, results))
    self.assertEqual(expected_notifications, actual_notifications,
        msg='Expected %d notifications, but got %d. Results: %s' %
        (expected_notifications, actual_notifications, results))

  def _CheckUpload(self, paths, deleted_paths=None):
    input_api = InputAPIStub(paths, deleted_paths)
    return PRESUBMIT.CheckChangeOnUpload(input_api, OutputAPIStub())

  def testIgnoreDeleted(self):
    results = self._CheckUpload([], ['/path/to/deleted.wpr.sha1'])
    self.assertResultCount(results, 0, 0)

  def testIgnoreNonHashes(self):
    results = self._CheckUpload(['/path/to/irrelevant.py'])
    self.assertResultCount(results, 0, 0)

  def testInvalidHash(self):
    results = self._CheckUpload(['/path/to/invalid_hash.wpr.sha1'])
    self.assertResultCount(results, 1, 0)
    self.assertTrue('valid SHA-1 hash' in str(results[0]), msg=results[0])

  def testMissingFile(self):
    results = self._CheckUpload(['/path/to/missing.wpr.sha1'])
    self.assertResultCount(results, 1, 0)
    self.assertTrue('not found' in str(results[0]), msg=results[0])

  def testSkip(self):
    results = self._CheckUpload(['/path/to/skip.wpr.sha1'])
    self.assertResultCount(results, 0, 0)

  def testSuccess(self):
    results = self._CheckUpload(['/path/to/success.wpr.sha1'])
    self.assertResultCount(results, 0, 1)
    self.assertTrue('Uploaded' in str(results[0]), msg=results[0])

  def testWrongHash(self):
    results = self._CheckUpload(['/path/to/wrong_hash.wpr.sha1'])
    self.assertTrue('does not match' in str(results[0]), msg=results[0])


if __name__ == '__main__':
  unittest.main()
