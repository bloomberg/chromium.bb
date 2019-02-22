# -*- coding: utf-8 -*-
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test the gslib module."""

from __future__ import print_function

import errno
import mox

from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib.paygen import gslib


# Typical output for a GS failure that is not our fault, and we should retry.
GS_RETRY_FAILURE = ('GSResponseError: status=403, code=InvalidAccessKeyId,'
                    'reason="Forbidden", message="Blah Blah Blah"')
# Typical output for a failure that we should not retry.
GS_DONE_FAILURE = ('AccessDeniedException:')


class TestGsLib(cros_test_lib.MoxTestCase):
  """Test gslib module."""

  def setUp(self):
    self.bucket_name = 'somebucket'
    self.bucket_uri = 'gs://%s' % self.bucket_name

    self.cmd_result = cros_build_lib.CommandResult()
    self.cmd_error = cros_build_lib.RunCommandError('', self.cmd_result)

    self.mox.StubOutWithMock(cros_build_lib, 'RunCommand')
    # Because of autodetection, we no longer know which gsutil binary
    # will be used.
    self.gsutil = mox.IsA(str)

  def testRetryGSLib(self):
    """Test our retry decorator"""
    @gslib.RetryGSLib
    def Success():
      pass

    @gslib.RetryGSLib
    def SuccessArguments(arg1, arg2=False, arg3=False):
      self.assertEqual(arg1, 1)
      self.assertEqual(arg2, 2)
      self.assertEqual(arg3, 3)

    class RetryTestException(gslib.GSLibError):
      """Testing gslib.GSLibError exception for Retrying cases."""

      def __init__(self):
        super(RetryTestException, self).__init__(GS_RETRY_FAILURE)

    class DoneTestException(gslib.GSLibError):
      """Testing gslib.GSLibError exception for Done cases."""

      def __init__(self):
        super(DoneTestException, self).__init__(GS_DONE_FAILURE)

    @gslib.RetryGSLib
    def Fail():
      raise RetryTestException()

    @gslib.RetryGSLib
    def FailCount(counter, exception):
      """Pass in [count] times to fail before passing.

      Using [] means the same object is used each retry, but it's contents
      are mutable.
      """
      counter[0] -= 1
      if counter[0] >= 0:
        raise exception()

      if exception == RetryTestException:
        # Make sure retries ran down to -1.
        self.assertEquals(-1, counter[0])

    Success()
    SuccessArguments(1, 2, 3)
    SuccessArguments(1, arg3=3, arg2=2)

    FailCount([1], RetryTestException)
    FailCount([2], RetryTestException)

    self.assertRaises(RetryTestException, Fail)
    self.assertRaises(DoneTestException, FailCount, [1], DoneTestException)
    self.assertRaises(gslib.CopyFail, FailCount, [3], gslib.CopyFail)
    self.assertRaises(gslib.CopyFail, FailCount, [4], gslib.CopyFail)

  def testRunGsutilCommand(self):
    args = ['TheCommand', 'Arg1', 'Arg2']
    cmd = [self.gsutil] + args

    # Set up the test replay script.
    # Run 1.
    cros_build_lib.RunCommand(
        cmd, redirect_stdout=True, redirect_stderr=True).AndReturn(1)
    # Run 2.
    cros_build_lib.RunCommand(
        cmd, redirect_stdout=False, redirect_stderr=True).AndReturn(2)
    # Run 3.
    cros_build_lib.RunCommand(cmd, redirect_stdout=True, redirect_stderr=True,
                              error_code_ok=True).AndReturn(3)
    # Run 4.
    cros_build_lib.RunCommand(
        cmd, redirect_stdout=True, redirect_stderr=True).AndRaise(
            self.cmd_error)
    # Run 5.
    cros_build_lib.RunCommand(
        cmd, redirect_stdout=True, redirect_stderr=True).AndRaise(
            OSError(errno.ENOENT, 'errmsg'))
    self.mox.ReplayAll()

    # Run the test verification.
    self.assertEqual(1, gslib.RunGsutilCommand(args))
    self.assertEqual(2, gslib.RunGsutilCommand(args, redirect_stdout=False))
    self.assertEqual(3, gslib.RunGsutilCommand(args, error_code_ok=True))
    self.assertRaises(gslib.GSLibError, gslib.RunGsutilCommand, args)
    self.assertRaises(gslib.GsutilMissingError, gslib.RunGsutilCommand, args)
    self.mox.VerifyAll()

  def testCopy(self):
    src_path = '/path/to/some/file'
    dest_path = 'gs://bucket/some/gs/path'

    # Set up the test replay script.
    # Run 1, success.
    cmd = [self.gsutil, 'cp', src_path, dest_path]
    cros_build_lib.RunCommand(cmd, redirect_stdout=True, redirect_stderr=True)
    # Run 2, failure.
    error = cros_build_lib.RunCommandError(GS_RETRY_FAILURE, self.cmd_result)
    for _ix in xrange(gslib.RETRY_ATTEMPTS + 1):
      cmd = [self.gsutil, 'cp', src_path, dest_path]
      cros_build_lib.RunCommand(
          cmd, redirect_stdout=True, redirect_stderr=True).AndRaise(error)
    self.mox.ReplayAll()

    # Run the test verification.
    gslib.Copy(src_path, dest_path)
    self.assertRaises(gslib.CopyFail, gslib.Copy, src_path, dest_path)
    self.mox.VerifyAll()

  def testCat(self):
    path = 'gs://bucket/some/gs/path'

    # Set up the test replay script.
    cmd = [self.gsutil, 'cat', path]
    result = cros_test_lib.EasyAttr(error='', output='TheContent')
    cros_build_lib.RunCommand(
        cmd, redirect_stdout=True, redirect_stderr=True).AndReturn(result)
    self.mox.ReplayAll()

    # Run the test verification.
    result = gslib.Cat(path)
    self.assertEquals('TheContent', result)
    self.mox.VerifyAll()

  def testCatFail(self):
    path = 'gs://bucket/some/gs/path'

    # Set up the test replay script.
    cmd = [self.gsutil, 'cat', path]
    for _ix in xrange(gslib.RETRY_ATTEMPTS + 1):
      cros_build_lib.RunCommand(
          cmd, redirect_stdout=True, redirect_stderr=True).AndRaise(
              self.cmd_error)
    self.mox.ReplayAll()

    # Run the test verification.
    self.assertRaises(gslib.CatFail, gslib.Cat, path)
    self.mox.VerifyAll()

  def testStat(self):
    path = 'gs://bucket/some/gs/path'

    # Set up the test replay script.
    cmd = [self.gsutil, 'stat', path]
    cros_build_lib.RunCommand(cmd, redirect_stdout=True,
                              redirect_stderr=True).AndReturn(self.cmd_result)
    self.mox.ReplayAll()

    # Run the test verification.
    self.assertIs(gslib.Stat(path), None)
    self.mox.VerifyAll()

  def testStatFail(self):
    path = 'gs://bucket/some/gs/path'

    # Set up the test replay script.
    cmd = [self.gsutil, 'stat', path]
    cros_build_lib.RunCommand(
        cmd, redirect_stdout=True, redirect_stderr=True).AndRaise(
            self.cmd_error)
    self.mox.ReplayAll()

    # Run the test verification.
    self.assertRaises(gslib.StatFail, gslib.Stat, path)
    self.mox.VerifyAll()

  def _TestCatWithHeaders(self, gs_uri, cmd_output, cmd_error):
    # Set up the test replay script.
    # Run 1, versioning not enabled in bucket, one line of output.
    cmd = ['gsutil', '-d', 'cat', gs_uri]
    cmd_result = cros_test_lib.EasyAttr(output=cmd_output,
                                        error=cmd_error,
                                        cmdstr=' '.join(cmd))
    cmd[0] = mox.IsA(str)
    cros_build_lib.RunCommand(
        cmd, redirect_stdout=True, redirect_stderr=True).AndReturn(cmd_result)
    self.mox.ReplayAll()

  def testCatWithHeaders(self):
    gs_uri = '%s/%s' % (self.bucket_uri, 'some/file/path')
    generation = 123454321
    metageneration = 2
    error = '\n'.join([
        'header: x-goog-generation: %d' % generation,
        'header: x-goog-metageneration: %d' % metageneration,
    ])
    expected_output = 'foo'
    self._TestCatWithHeaders(gs_uri, expected_output, error)

    # Run the test verification.
    headers = {}
    result = gslib.Cat(gs_uri, headers=headers)
    self.assertEqual(generation, int(headers['generation']))
    self.assertEqual(metageneration, int(headers['metageneration']))
    self.assertEqual(result, expected_output)
    self.mox.VerifyAll()

  def testListFiles(self):
    files = [
        '%s/some/path' % self.bucket_uri,
        '%s/some/file/path' % self.bucket_uri,
    ]
    directories = [
        '%s/some/dir/' % self.bucket_uri,
        '%s/some/dir/path/' % self.bucket_uri,
    ]

    gs_uri = '%s/**' % self.bucket_uri
    cmd = [self.gsutil, 'ls', gs_uri]

    # Prepare cmd_result for a good run.
    # Fake a trailing empty line.
    output = '\n'.join(files + directories + [''])
    cmd_result_ok = cros_test_lib.EasyAttr(output=output, returncode=0)

    # Prepare exception for a run that finds nothing.
    stderr = 'CommandException: One or more URLs matched no objects.\n'
    cmd_result_empty = cros_build_lib.CommandResult(error=stderr)
    empty_exception = cros_build_lib.RunCommandError(stderr, cmd_result_empty)

    # Prepare exception for a run that triggers a GS failure.
    error = cros_build_lib.RunCommandError(GS_RETRY_FAILURE, self.cmd_result)

    # Set up the test replay script.
    # Run 1, runs ok.
    cros_build_lib.RunCommand(
        cmd, redirect_stdout=True, redirect_stderr=True).AndReturn(
            cmd_result_ok)
    # Run 2, runs ok, sorts files.
    cros_build_lib.RunCommand(
        cmd, redirect_stdout=True, redirect_stderr=True).AndReturn(
            cmd_result_ok)
    # Run 3, finds nothing.
    cros_build_lib.RunCommand(
        cmd, redirect_stdout=True, redirect_stderr=True).AndRaise(
            empty_exception)
    # Run 4, failure in GS.
    for _ix in xrange(gslib.RETRY_ATTEMPTS + 1):
      cros_build_lib.RunCommand(
          cmd, redirect_stdout=True, redirect_stderr=True).AndRaise(error)
    self.mox.ReplayAll()

    # Run the test verification.
    result = gslib.ListFiles(self.bucket_uri, recurse=True)
    self.assertEqual(files, result)
    result = gslib.ListFiles(self.bucket_uri, recurse=True, sort=True)
    self.assertEqual(sorted(files), result)
    result = gslib.ListFiles(self.bucket_uri, recurse=True)
    self.assertEqual([], result)
    self.assertRaises(gslib.GSLibError, gslib.ListFiles,
                      self.bucket_uri, recurse=True)
    self.mox.VerifyAll()
