# -*- coding: utf-8 -*-
# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test suite for timeout_util.py"""

from __future__ import print_function

import datetime
from multiprocessing.pool import ThreadPool
import signal
import time

from chromite.lib import cros_test_lib
from chromite.lib import timeout_util


class TestTimeouts(cros_test_lib.MockTestCase):
  """Tests for timeout_util.Timeout."""

  def testTimeout(self):
    """Tests that we can nest Timeout correctly."""
    self.assertFalse('mock' in str(time.sleep).lower())
    with timeout_util.Timeout(30):
      with timeout_util.Timeout(20):
        with timeout_util.Timeout(1):
          self.assertRaises(timeout_util.TimeoutError, time.sleep, 10)

        # Should not raise a timeout exception as 20 > 2.
        time.sleep(1)

  def testTimeoutNested(self):
    """Tests that we still re-raise an alarm if both are reached."""
    with timeout_util.Timeout(1):
      try:
        with timeout_util.Timeout(2):
          self.assertRaises(timeout_util.TimeoutError, time.sleep, 1)

      # Craziness to catch nested timeouts.
      except timeout_util.TimeoutError:
        pass
      else:
        self.fail('Should have thrown an exception')

  def testFractionTimeout(self):
    # Capture setitimer arguments.
    mock_setitimer = self.PatchObject(
        signal, 'setitimer', autospec=True, return_value=(0, 0))
    with timeout_util.Timeout(0.5):
      pass

    # The timeout should be fraction, rather than rounding up to int seconds.
    self.assertEqual(mock_setitimer.call_args_list,
                     [((signal.ITIMER_REAL, 0),),
                      ((signal.ITIMER_REAL, 0.5, 0),),
                      ((signal.ITIMER_REAL, 0),)])


class TestTimeoutDecorator(cros_test_lib.TestCase):
  """Tests timeout_util.TimeoutDecorator."""

  def testNoTimeout(self):
    """Test normal class with no timeout."""

    @timeout_util.TimeoutDecorator(10)
    def timedFunction(a, b):
      return a + b

    result = timedFunction(1, 2)

    self.assertEqual(result, 3)

  def testTimeout(self):
    """Test timing out a function."""

    @timeout_util.TimeoutDecorator(1)
    def timedFunction():
      time.sleep(10)

    with self.assertRaises(timeout_util.TimeoutError):
      timedFunction()


class TestWaitFors(cros_test_lib.TestCase):
  """Tests for assorted timeout_utils WaitForX methods."""

  def setUp(self):
    self.values_ix = 0
    self.timestart = None
    self.timestop = None

  def GetFunc(self, return_values):
    """Return a functor that returns given values in sequence with each call."""
    self.values_ix = 0
    self.timestart = None
    self.timestop = None

    def _Func():
      if not self.timestart:
        self.timestart = datetime.datetime.utcnow()

      val = return_values[self.values_ix]
      self.values_ix += 1

      self.timestop = datetime.datetime.utcnow()
      return val

    return _Func

  def GetTryCount(self):
    """Get number of times func was tried."""
    return self.values_ix

  def GetTrySeconds(self):
    """Get number of seconds that span all func tries."""
    delta = self.timestop - self.timestart
    return int(delta.seconds + 0.5)

  def _TestWaitForSuccess(self, maxval, timeout, **kwargs):
    """Run through a test for WaitForSuccess."""

    func = self.GetFunc(list(range(20)))
    def _RetryCheck(val):
      return val < maxval

    return timeout_util.WaitForSuccess(_RetryCheck, func, timeout, **kwargs)

  def _TestWaitForReturnValue(self, values, timeout, **kwargs):
    """Run through a test for WaitForReturnValue."""
    func = self.GetFunc(list(range(20)))
    return timeout_util.WaitForReturnValue(values, func, timeout, **kwargs)

  def testWaitForSuccessNotMainThread(self):
    """Test success after a few tries."""
    pool = ThreadPool(processes=1)
    async_result = pool.apply_async(self._TestWaitForSuccess, (4, 10, ),
                                    {'period': 1})
    return_val = async_result.get()
    self.assertEqual(4, return_val)
    self.assertEqual(5, self.GetTryCount())
    self.assertEqual(4, self.GetTrySeconds())

  def testWaitForSuccess1(self):
    """Test success after a few tries."""
    self.assertEqual(4, self._TestWaitForSuccess(4, 10, period=1))
    self.assertEqual(5, self.GetTryCount())
    self.assertEqual(4, self.GetTrySeconds())

  def testWaitForSuccess2(self):
    """Test timeout after a couple tries."""
    self.assertRaises(timeout_util.TimeoutError, self._TestWaitForSuccess,
                      4, 3, period=1)
    self.assertEqual(3, self.GetTryCount())
    self.assertEqual(2, self.GetTrySeconds())

  def testWaitForSuccess3(self):
    """Test success on first try."""
    self.assertEqual(0, self._TestWaitForSuccess(0, 10, period=1))
    self.assertEqual(1, self.GetTryCount())
    self.assertEqual(0, self.GetTrySeconds())

  def testWaitForSuccess4(self):
    """Test success after a few tries with longer period."""
    self.assertEqual(3, self._TestWaitForSuccess(3, 10, period=2))
    self.assertEqual(4, self.GetTryCount())
    self.assertEqual(6, self.GetTrySeconds())

  def testWaitForReturnValue1(self):
    """Test value found after a few tries."""
    self.assertEqual(4, self._TestWaitForReturnValue((4, 5), 10, period=1))
    self.assertEqual(5, self.GetTryCount())
    self.assertEqual(4, self.GetTrySeconds())

  def testWaitForReturnValue2(self):
    """Test value found on first try."""
    self.assertEqual(0, self._TestWaitForReturnValue((0, 1), 10, period=1))
    self.assertEqual(1, self.GetTryCount())
    self.assertEqual(0, self.GetTrySeconds())

  def testWaitForCallback(self):
    """Verify side_effect_func works."""
    side_effect_called = [False]
    def _SideEffect(remaining):
      self.assertTrue(isinstance(remaining, datetime.timedelta))
      side_effect_called[0] = True
    self.assertEqual(1, self._TestWaitForSuccess(
        1, 10, period=0.1, side_effect_func=_SideEffect))
    self.assertTrue(side_effect_called[0])

  def testWaitForCallbackSleepsLong(self):
    """Verify a long running side effect doesn't call time.sleep(<negative>)."""
    side_effect_called = [False]
    def _SideEffect(_remaining):
      time.sleep(0.3)
      side_effect_called[0] = True
    self.assertRaises(timeout_util.TimeoutError, self._TestWaitForSuccess,
                      10, 0, period=0.1, side_effect_func=_SideEffect)
    self.assertTrue(side_effect_called[0])

  def testWaitForCallbackAfterTimeout(self):
    """If side_effect is called after the timeout, remaining should be zero."""
    side_effect_called = [False]
    def _SideEffect(remaining):
      self.assertGreaterEqual(remaining.total_seconds(), 0)
      side_effect_called[0] = True
    self.assertRaises(timeout_util.TimeoutError, self._TestWaitForSuccess,
                      10, 0, period=0.1, side_effect_func=_SideEffect)
    self.assertTrue(side_effect_called[0])
