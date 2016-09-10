# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import functools
import threading
import time
import unittest

import mock

from testing_support import auto_stub

from infra_libs.ts_mon.common import errors
from infra_libs.ts_mon.common import interface
from infra_libs.ts_mon.common import metrics
from infra_libs.ts_mon.common.test import stubs


class GlobalsTest(unittest.TestCase):

  def setUp(self):
    self.mock_state = stubs.MockState()
    self.state_patcher = mock.patch('infra_libs.ts_mon.common.interface.state',
                                    new=self.mock_state)
    self.state_patcher.start()

  def tearDown(self):
    # It's important to call close() before un-setting the mock state object,
    # because any FlushThread started by the test is stored in that mock state
    # and needs to be stopped before running any other tests.
    interface.close()
    self.state_patcher.stop()

  def test_flush(self):
    interface.state.global_monitor = stubs.MockMonitor()
    interface.state.target = stubs.MockTarget()

    # pylint: disable=unused-argument
    def serialize_to(pb, start_time, fields, value, target):
      pb.data.add().name = 'foo'

    fake_metric = mock.create_autospec(metrics.Metric, spec_set=True)
    fake_metric.name = 'fake'
    fake_metric.serialize_to.side_effect = serialize_to
    interface.register(fake_metric)
    interface.state.store.set('fake', (), 123)

    interface.flush()
    interface.state.global_monitor.send.assert_called_once()
    proto = interface.state.global_monitor.send.call_args[0][0]
    self.assertEqual(1, len(proto.data))
    self.assertEqual('foo', proto.data[0].name)

  def test_flush_raises(self):
    self.assertIsNone(interface.state.global_monitor)
    with self.assertRaises(errors.MonitoringNoConfiguredMonitorError):
      interface.flush()

  def test_flush_many(self):
    interface.state.global_monitor = stubs.MockMonitor()
    interface.state.target = stubs.MockTarget()

    # pylint: disable=unused-argument
    def serialize_to(pb, start_time, fields, value, target):
      pb.data.add().name = 'foo'

    # We can't use the mock's call_args_list here because the same object is
    # reused as the argument to both calls and cleared inbetween.
    data_lengths = []
    def send(proto):
      data_lengths.append(len(proto.data))
    interface.state.global_monitor.send.side_effect = send

    fake_metric = mock.create_autospec(metrics.Metric, spec_set=True)
    fake_metric.name = 'fake'
    fake_metric.serialize_to.side_effect = serialize_to
    interface.register(fake_metric)

    for i in xrange(1001):
      interface.state.store.set('fake', ('field', i), 123)

    interface.flush()
    self.assertEquals(2, interface.state.global_monitor.send.call_count)
    self.assertEqual(1000, data_lengths[0])
    self.assertEqual(1, data_lengths[1])

  def test_register_unregister(self):
    fake_metric = mock.create_autospec(metrics.Metric, spec_set=True)
    self.assertEqual(0, len(interface.state.metrics))
    interface.register(fake_metric)
    self.assertEqual(1, len(interface.state.metrics))
    interface.unregister(fake_metric)
    self.assertEqual(0, len(interface.state.metrics))

  def test_identical_register(self):
    fake_metric = mock.Mock(_name='foo')
    interface.register(fake_metric)
    interface.register(fake_metric)
    self.assertEqual(1, len(interface.state.metrics))

  def test_duplicate_register_raises(self):
    fake_metric = mock.Mock()
    fake_metric.name = 'foo'
    phake_metric = mock.Mock()
    phake_metric.name = 'foo'
    interface.register(fake_metric)
    with self.assertRaises(errors.MonitoringDuplicateRegistrationError):
      interface.register(phake_metric)
    self.assertEqual(1, len(interface.state.metrics))

  def test_unregister_missing_raises(self):
    fake_metric = mock.Mock(_name='foo')
    self.assertEqual(0, len(interface.state.metrics))
    with self.assertRaises(KeyError):
      interface.unregister(fake_metric)

  def test_close_stops_flush_thread(self):
    interface.state.flush_thread = interface._FlushThread(10)
    interface.state.flush_thread.start()

    self.assertTrue(interface.state.flush_thread.is_alive())
    interface.close()
    self.assertFalse(interface.state.flush_thread.is_alive())

  def test_reset_for_unittest(self):
    metric = metrics.CounterMetric('foo')
    metric.increment()
    self.assertEquals(1, metric.get())

    interface.reset_for_unittest()
    self.assertIsNone(metric.get())


class FakeThreadingEvent(object):
  """A fake threading.Event that doesn't use the clock for timeouts."""

  def __init__(self):
    # If not None, called inside wait() with the timeout (in seconds) to
    # increment a fake clock.
    self.increment_time_func = None

    self._is_set = False  # Return value of the next call to wait.
    self._last_wait_timeout = None  # timeout argument of the last call to wait.

    self._wait_enter_semaphore = threading.Semaphore(0)
    self._wait_exit_semaphore = threading.Semaphore(0)

  def timeout_wait(self):
    """Blocks until the next time the code under test calls wait().

    Makes the wait() call return False (indicating a timeout), and this call
    returns the timeout argument given to the wait() method.

    Called by the test.
    """

    self._wait_enter_semaphore.release()
    self._wait_exit_semaphore.acquire()
    return self._last_wait_timeout

  def set(self, blocking=True):
    """Makes the next wait() call return True.

    By default this blocks until the next call to wait(), but you can pass
    blocking=False to just set the flag, wake up any wait() in progress (if any)
    and return immediately.
    """

    self._is_set = True
    self._wait_enter_semaphore.release()
    if blocking:
      self._wait_exit_semaphore.acquire()

  def wait(self, timeout):
    """Block until either set() or timeout_wait() is called by the test."""

    self._wait_enter_semaphore.acquire()
    self._last_wait_timeout = timeout
    if self.increment_time_func is not None:  # pragma: no cover
      self.increment_time_func(timeout)
    ret = self._is_set
    self._wait_exit_semaphore.release()
    return ret


class FlushThreadTest(unittest.TestCase):

  def setUp(self):
    mock.patch('infra_libs.ts_mon.common.interface.flush',
               autospec=True).start()
    mock.patch('time.time', autospec=True).start()

    self.fake_time = 0
    time.time.side_effect = lambda: self.fake_time

    self.stop_event = FakeThreadingEvent()
    self.stop_event.increment_time_func = self.increment_time

    self.t = interface._FlushThread(60, stop_event=self.stop_event)

  def increment_time(self, delta):
    self.fake_time += delta

  def assertInRange(self, lower, upper, value):
    self.assertGreaterEqual(value, lower)
    self.assertLessEqual(value, upper)

  def tearDown(self):
    # Ensure the thread exits.
    self.stop_event.set(blocking=False)
    self.t.join()

    mock.patch.stopall()

  def test_run_calls_flush(self):
    self.t.start()

    self.assertEqual(0, interface.flush.call_count)

    # The wait is for the whole interval (with jitter).
    self.assertInRange(30, 60, self.stop_event.timeout_wait())

    # Return from the second wait, which exits the thread.
    self.stop_event.set()
    self.t.join()
    self.assertEqual(2, interface.flush.call_count)

  def test_run_catches_exceptions(self):
    interface.flush.side_effect = Exception()
    self.t.start()

    self.stop_event.timeout_wait()
    # flush is called now and raises an exception.  The exception is caught, so
    # wait is called again.

    # Do it again to make sure the exception doesn't terminate the loop.
    self.stop_event.timeout_wait()

    # Return from the third wait, which exits the thread.
    self.stop_event.set()
    self.t.join()
    self.assertEqual(3, interface.flush.call_count)

  def test_stop_stops(self):
    self.t.start()

    self.assertTrue(self.t.is_alive())

    self.t.stop()
    self.assertFalse(self.t.is_alive())
    self.assertEqual(1, interface.flush.call_count)

  def test_sleeps_for_exact_interval(self):
    self.t.start()

    # Flush takes 5 seconds.
    interface.flush.side_effect = functools.partial(self.increment_time, 5)

    self.assertInRange(30, 60, self.stop_event.timeout_wait())
    self.assertAlmostEqual(55, self.stop_event.timeout_wait())
    self.assertAlmostEqual(55, self.stop_event.timeout_wait())

  def test_sleeps_for_minimum_zero_secs(self):
    self.t.start()

    # Flush takes 65 seconds.
    interface.flush.side_effect = functools.partial(self.increment_time, 65)

    self.assertInRange(30, 60, self.stop_event.timeout_wait())
    self.assertAlmostEqual(0, self.stop_event.timeout_wait())
    self.assertAlmostEqual(0, self.stop_event.timeout_wait())
