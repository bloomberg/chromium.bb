# -*- coding: utf-8 -*-
# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test the failures_lib module."""

from __future__ import print_function

import json

from chromite.lib import failures_lib
from chromite.lib import failure_message_lib
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib


class StepFailureTests(cros_test_lib.TestCase):
  """Tests for StepFailure."""

  def testConvertToStageFailureMessage(self):
    """Test ConvertToStageFailureMessage."""
    failure = failures_lib.StepFailure(message='step failure message')
    stage_failure_msg = failure.ConvertToStageFailureMessage(
        1, 'HWTest [sanity]')
    self.assertEqual(stage_failure_msg.stage_name, 'HWTest [sanity]')
    self.assertEqual(stage_failure_msg.stage_prefix_name, 'HWTest')
    self.assertEqual(stage_failure_msg.exception_type, 'StepFailure')
    self.assertEqual(stage_failure_msg.exception_category, 'unknown')


class CompoundFailureTest(cros_test_lib.TestCase):
  """Test the CompoundFailure class."""

  def _CreateExceptInfos(self, cls, message='', traceback='', num=1):
    """A helper function to create a list of ExceptInfo objects."""
    exc_infos = []
    for _ in range(num):
      exc_infos.extend(failures_lib.CreateExceptInfo(cls(message), traceback))

    return exc_infos

  def testHasEmptyList(self):
    """Tests the HasEmptyList method."""
    self.assertTrue(failures_lib.CompoundFailure().HasEmptyList())
    exc_infos = self._CreateExceptInfos(KeyError)
    self.assertFalse(
        failures_lib.CompoundFailure(exc_infos=exc_infos).HasEmptyList())

  def testHasAndMatchesFailureType(self):
    """Tests the HasFailureType and the MatchesFailureType methods."""
    # Create a CompoundFailure instance with mixed types of exceptions.
    exc_infos = self._CreateExceptInfos(KeyError)
    exc_infos.extend(self._CreateExceptInfos(ValueError))
    exc = failures_lib.CompoundFailure(exc_infos=exc_infos)
    self.assertTrue(exc.HasFailureType(KeyError))
    self.assertTrue(exc.HasFailureType(ValueError))
    self.assertFalse(exc.MatchesFailureType(KeyError))
    self.assertFalse(exc.MatchesFailureType(ValueError))

    # Create a CompoundFailure instance with a single type of exceptions.
    exc_infos = self._CreateExceptInfos(KeyError, num=5)
    exc = failures_lib.CompoundFailure(exc_infos=exc_infos)
    self.assertTrue(exc.HasFailureType(KeyError))
    self.assertFalse(exc.HasFailureType(ValueError))
    self.assertTrue(exc.MatchesFailureType(KeyError))
    self.assertFalse(exc.MatchesFailureType(ValueError))

  def testHasFatalFailure(self):
    """Tests the HasFatalFailure method."""
    exc_infos = self._CreateExceptInfos(KeyError)
    exc_infos.extend(self._CreateExceptInfos(ValueError))
    exc = failures_lib.CompoundFailure(exc_infos=exc_infos)
    self.assertTrue(exc.HasFatalFailure())
    self.assertTrue(exc.HasFatalFailure(whitelist=[KeyError]))
    self.assertFalse(exc.HasFatalFailure(whitelist=[KeyError, ValueError]))

    exc = failures_lib.CompoundFailure()
    self.assertFalse(exc.HasFatalFailure())

  def testMessageContainsAllInfo(self):
    """Tests that by default, all information is included in the message."""
    exc_infos = self._CreateExceptInfos(KeyError, message='bar1',
                                        traceback='foo1')
    exc_infos.extend(self._CreateExceptInfos(ValueError, message='bar2',
                                             traceback='foo2'))
    exc = failures_lib.CompoundFailure(exc_infos=exc_infos)
    self.assertTrue('bar1' in str(exc))
    self.assertTrue('bar2' in str(exc))
    self.assertTrue('KeyError' in str(exc))
    self.assertTrue('ValueError' in str(exc))
    self.assertTrue('foo1' in str(exc))
    self.assertTrue('foo2' in str(exc))

  def testConvertToStageFailureMessage(self):
    """Test ConvertToStageFailureMessage."""
    exc_infos = self._CreateExceptInfos(KeyError, message='bar1',
                                        traceback='foo1')
    exc_infos.extend(self._CreateExceptInfos(failures_lib.StepFailure,
                                             message='bar2',
                                             traceback='foo2'))
    exc = failures_lib.CompoundFailure(message='compound failure',
                                       exc_infos=exc_infos)
    stage_failure_msg = exc.ConvertToStageFailureMessage(1, 'HWTest [sanity]')

    self.assertEqual(len(stage_failure_msg.inner_failures), 2)
    self.assertEqual(stage_failure_msg.stage_name, 'HWTest [sanity]')
    self.assertEqual(stage_failure_msg.stage_prefix_name, 'HWTest')
    self.assertEqual(stage_failure_msg.exception_type, 'CompoundFailure')
    self.assertEqual(stage_failure_msg.exception_category, 'unknown')

class ReportStageFailureTest(cros_test_lib.MockTestCase):
  """Tests for ReportStageFailure."""

  def testReportStageFailure(self):
    """Test ReportStageFailure."""

    class FakeStepFailure(failures_lib.StepFailure):
      """A fake StepFailure subclass for unittest."""
      EXCEPTION_CATEGORY = 'unittest'

    fake_failure = FakeStepFailure('Toot! Toot!')
    insert_failure_fn = self.PatchObject(failures_lib,
                                         '_InsertFailureToMonarch')
    failures_lib.ReportStageFailure(
        fake_failure, {})
    insert_failure_fn.assert_called_once_with(exception_category='unittest',
                                              metrics_fields={})


class SetFailureTypeTest(cros_test_lib.TestCase):
  """Test that the SetFailureType decorator works."""
  ERROR_MESSAGE = 'You failed!'

  class TacoNotTasty(failures_lib.CompoundFailure):
    """Raised when the taco is not tasty."""

  class NoGuacamole(TacoNotTasty):
    """Raised when no guacamole in the taco."""

  class SubparLunch(failures_lib.CompoundFailure):
    """Raised when the lunch is subpar."""

  class FooException(Exception):
    """A foo exception."""

  def _GetFunction(self, set_type, raise_type, *args, **kwargs):
    """Returns a function to test.

    Args:
      set_type: The exception type that the function is decorated with.
      raise_type: The exception type that the function raises.
      *args: args to pass to the instance of |raise_type|.

    Returns:
      The function to test.
    """
    @failures_lib.SetFailureType(set_type)
    def f():
      raise raise_type(*args, **kwargs)

    return f

  def testAssertionFailOnIllegalExceptionType(self):
    """Assertion should fail if the pre-set type is not allowed ."""
    self.assertRaises(AssertionError, self._GetFunction, ValueError,
                      self.FooException)

  def testReraiseAsNewException(self):
    """Tests that the pre-set exception type is raised correctly."""
    try:
      self._GetFunction(self.TacoNotTasty, self.FooException,
                        self.ERROR_MESSAGE)()
    except Exception as e:
      self.assertTrue(isinstance(e, self.TacoNotTasty))
      self.assertTrue(e.message, self.ERROR_MESSAGE)
      self.assertEqual(len(e.exc_infos), 1)
      self.assertEqual(e.exc_infos[0].str, self.ERROR_MESSAGE)
      self.assertEqual(e.exc_infos[0].type, self.FooException)
      self.assertTrue(isinstance(e.exc_infos[0].traceback, str))

  def testReraiseACompoundFailure(self):
    """Tests that the list of ExceptInfo objects are copied over."""
    tb1 = 'Dummy traceback1'
    tb2 = 'Dummy traceback2'
    org_infos = failures_lib.CreateExceptInfo(ValueError('No taco.'), tb1) + \
                failures_lib.CreateExceptInfo(OSError('No salsa'), tb2)
    try:
      self._GetFunction(self.SubparLunch, self.TacoNotTasty,
                        exc_infos=org_infos)()
    except Exception as e:
      self.assertTrue(isinstance(e, self.SubparLunch))
      # The orignal exceptions stored in exc_infos are preserved.
      self.assertEqual(e.exc_infos, org_infos)
      # All essential inforamtion should be included in the message of
      # the new excpetion.
      self.assertTrue(tb1 in str(e))
      self.assertTrue(tb2 in str(e))
      self.assertTrue(str(ValueError) in str(e))
      self.assertTrue(str(OSError) in str(e))
      self.assertTrue(str('No taco') in str(e))
      self.assertTrue(str('No salsa') in str(e))

      # Assert that summary does not contain the textual tracebacks.
      self.assertFalse(tb1 in e.ToSummaryString())
      self.assertFalse(tb2 in e.ToSummaryString())

  def testReraiseACompoundFailureWithEmptyList(self):
    """Tests that a CompoundFailure with empty list is handled correctly."""
    try:
      self._GetFunction(self.SubparLunch, self.TacoNotTasty,
                        message='empty list')()
    except Exception as e:
      self.assertTrue(isinstance(e, self.SubparLunch))
      self.assertEqual(e.exc_infos[0].type, self.TacoNotTasty)

  def testReraiseOriginalException(self):
    """Tests that the original exception is re-raised."""
    # NoGuacamole is a subclass of TacoNotTasty, so the wrapper has no
    # effect on it.
    f = self._GetFunction(self.TacoNotTasty, self.NoGuacamole)
    self.assertRaises(self.NoGuacamole, f)

  def testPassArgsToWrappedFunctor(self):
    """Tests that we can pass arguments to the functor."""
    @failures_lib.SetFailureType(self.TacoNotTasty)
    def f(arg):
      return arg

    @failures_lib.SetFailureType(self.TacoNotTasty)
    def g(kwarg=''):
      return kwarg

    # Test passing arguments.
    self.assertEqual(f('foo'), 'foo')
    # Test passing keyword arguments.
    self.assertEqual(g(kwarg='bar'), 'bar')


class ExceptInfoTest(cros_test_lib.TestCase):
  """Tests the namedtuple class ExceptInfo."""

  def testConvertToExceptInfo(self):
    """Tests converting an exception to an ExceptInfo object."""
    traceback = 'Dummy traceback'
    message = 'Taco is not a valid option!'
    except_infos = failures_lib.CreateExceptInfo(
        ValueError(message), traceback)

    self.assertEqual(except_infos[0].type, ValueError)
    self.assertEqual(except_infos[0].str, message)
    self.assertEqual(except_infos[0].traceback, traceback)


class FailureTypeListTests(cros_test_lib.TestCase):
  """Tests for failure type lists."""

  def testFailureTypeList(self):
    """Test the current failure names are already added to the type lists."""
    self.assertTrue(failures_lib.BuildScriptFailure.__name__ in
                    failure_message_lib.BUILD_SCRIPT_FAILURE_TYPES)
    self.assertTrue(failures_lib.PackageBuildFailure.__name__ in
                    failure_message_lib.PACKAGE_BUILD_FAILURE_TYPES)


class GetStageFailureMessageFromExceptionTests(cros_test_lib.TestCase):
  """Tests for GetStageFailureMessageFromException"""

  def testGetStageFailureMessageFromExceptionOnStepFailure(self):
    """Test GetStageFailureMessageFromException on StepFailure."""
    exc = failures_lib.StepFailure(message='step failure message')
    msg = failures_lib.GetStageFailureMessageFromException(
        'CommitQueueSync', 1, exc)
    self.assertEqual(msg.build_stage_id, 1)
    self.assertEqual(msg.stage_name, 'CommitQueueSync')
    self.assertEqual(msg.stage_prefix_name, 'CommitQueueSync')
    self.assertEqual(msg.exception_type, 'StepFailure')
    self.assertEqual(msg.exception_category, 'unknown')

  def testGetStageFailureMessageFromExceptionOnException(self):
    """Test GetStageFailureMessageFromException on regular exception."""
    exc = ValueError('Invalid valure.')
    msg = failures_lib.GetStageFailureMessageFromException(
        'CommitQueueSync', 1, exc)
    self.assertEqual(msg.build_stage_id, 1)
    self.assertEqual(msg.stage_name, 'CommitQueueSync')
    self.assertEqual(msg.stage_prefix_name, 'CommitQueueSync')
    self.assertEqual(msg.exception_type, 'ValueError')
    self.assertEqual(msg.exception_category, 'unknown')


class BuildFailuresForFindit(cros_test_lib.TestCase):
  """Test cases for exporting build failures for Findit integration."""

  def testBuildFailuresJson(self):
    error = cros_build_lib.RunCommandError('run cmd error',
                                           cros_build_lib.CommandResult())
    failed_packages = ['sys-apps/mosys', 'chromeos-base/cryptohome']
    build_failure = failures_lib.PackageBuildFailure(
        error, './build_packages', failed_packages)
    self.assertSetEqual(set(failed_packages), build_failure.failed_packages)
    failure_json = build_failure.BuildCompileFailureOutputJson()
    values = json.loads(failure_json)
    failures = values['failures']
    self.assertEqual(len(failures), 2)
    # Verify both output targets are not equal, this makes sure the loop
    # below is correct.
    self.assertNotEqual(failures[0]['output_targets'],
                        failures[1]['output_targets'])
    for value in failures:
      self.assertEqual(value['rule'], 'emerge')
      self.assertIn(value['output_targets'], failed_packages)
