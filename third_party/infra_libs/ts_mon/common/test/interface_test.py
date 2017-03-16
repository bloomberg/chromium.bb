# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import functools
import threading
import time
import unittest

import mock

from testing_support import auto_stub

from infra_libs.ts_mon.common import distribution
from infra_libs.ts_mon.common import errors
from infra_libs.ts_mon.common import interface
from infra_libs.ts_mon.common import metric_store
from infra_libs.ts_mon.common import metrics
from infra_libs.ts_mon.common import targets
from infra_libs.ts_mon.common.test import stubs
from infra_libs.ts_mon.protos.new import metrics_pb2 as new_metrics_pb2


class GlobalsTest(unittest.TestCase):

  def setUp(self):
    target = targets.TaskTarget('test_service', 'test_job',
                                'test_region', 'test_host')
    self.mock_state = interface.State(target=target)
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
      pb.name = 'foo'

    fake_metric = mock.create_autospec(metrics.Metric, spec_set=True)
    fake_metric.name = 'fake'
    fake_metric.serialize_to.side_effect = serialize_to
    interface.register(fake_metric)
    interface.state.store.set('fake', (), None, 123)

    interface.flush()
    interface.state.global_monitor.send.assert_called_once()
    proto = interface.state.global_monitor.send.call_args[0][0]
    self.assertEqual(1, len(proto.data))
    self.assertEqual('foo', proto.data[0].name)

  def test_flush_empty(self):
    interface.state.global_monitor = stubs.MockMonitor()
    interface.state.target = stubs.MockTarget()

    interface.flush()
    interface.state.global_monitor.send.assert_not_called()

  def test_flush_corrupt_metric(self):
    """Test that a corrupt metric does not affect flushing other metrics."""
    interface.state.global_monitor = stubs.MockMonitor()

    good_metric = metrics.GaugeMetric('good_metric')
    bad_metric = metrics.GaugeMetric('bad_metric')
    interface.register(good_metric)
    interface.register(bad_metric)
    good_metric.set(1, fields={'valid_field': 1})
    bad_metric.set(2, fields={'invalid_field': mock.Mock()})

    with self.assertRaises(errors.MonitoringFailedToFlushAllMetricsError) as e:
      interface.flush()
    self.assertEqual(1, e.exception.error_count)

    interface.state.global_monitor.send.assert_called_once()
    proto = interface.state.global_monitor.send.call_args[0][0]
    self.assertEqual(1, len(proto.data))
    self.assertEqual('good_metric', proto.data[0].name)

  def test_flush_new(self):
    interface.state.metric_name_prefix = '/infra/test/'
    interface.state.global_monitor = stubs.MockMonitor()
    interface.state.target = targets.TaskTarget('a', 'b', 'c', 'd', 1)
    interface.state.use_new_proto = True

    counter = metrics.CounterMetric('counter', description='desc')
    interface.register(counter)
    counter.increment_by(3, {'test': 123})

    interface.flush()
    interface.state.global_monitor.send.assert_called_once()

    proto = interface.state.global_monitor.send.call_args[0][0]
    self.assertEqual(1, len(proto.metrics_collection))
    self.assertEqual(1, len(proto.metrics_collection[0].metrics_data_set))

    data_set = proto.metrics_collection[0].metrics_data_set[0]
    self.assertEqual('/infra/test/counter', data_set.metric_name)

  def test_flush_corrupt_metric_new(self):
    """Test that a corrupt metric does not affect flushing other metrics."""
    interface.state.global_monitor = stubs.MockMonitor()
    interface.state.use_new_proto = True

    bad_metric = metrics.GaugeMetric('bad_metric')
    partially_good_metric = metrics.GaugeMetric('partially_good_metric')
    interface.register(bad_metric)
    interface.register(partially_good_metric)
    bad_metric.set(1, fields={'bad_field_1': mock.Mock()})
    partially_good_metric.set(2, fields={'good_field': 22})
    partially_good_metric.set(3, fields={'bad_field_2': mock.Mock()})

    with self.assertRaises(errors.MonitoringFailedToFlushAllMetricsError) as e:
      interface.flush()
    self.assertEqual(2, e.exception.error_count)

    interface.state.global_monitor.send.assert_called_once()
    proto = interface.state.global_monitor.send.call_args[0][0]
    self.assertEqual(1, len(proto.metrics_collection))
    self.assertEqual(1, len(proto.metrics_collection[0].metrics_data_set))

    data_set = proto.metrics_collection[0].metrics_data_set[0]
    self.assertEqual(1, len(data_set.data))

    data = data_set.data[0]
    self.assertEqual(2, data.int64_value)
    self.assertEqual(1, len(data.field))
    self.assertEqual('good_field', data.field[0].name)

  def test_flush_empty_new(self):
    interface.state.metric_name_prefix = '/infra/test/'
    interface.state.global_monitor = stubs.MockMonitor()
    interface.state.target = targets.TaskTarget('a', 'b', 'c', 'd', 1)
    interface.state.use_new_proto = True

    interface.flush()
    interface.state.global_monitor.send.assert_not_called()

  def test_flush_disabled(self):
    interface.reset_for_unittest(disable=True)
    interface.state.global_monitor = stubs.MockMonitor()
    interface.state.target = stubs.MockTarget()
    interface.flush()
    self.assertFalse(interface.state.global_monitor.send.called)

  def test_flush_raises(self):
    self.assertIsNone(interface.state.global_monitor)
    with self.assertRaises(errors.MonitoringNoConfiguredMonitorError):
      interface.flush()

  def test_flush_many(self):
    interface.state.global_monitor = stubs.MockMonitor()
    interface.state.target = stubs.MockTarget()

    # pylint: disable=unused-argument
    def serialize_to(pb, start_time, fields, value, target):
      pb.name = 'foo'

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

    for i in xrange(501):
      interface.state.store.set('fake', ('field', i), None, 123)

    interface.flush()
    self.assertEquals(2, interface.state.global_monitor.send.call_count)
    self.assertListEqual([500, 1], data_lengths)

  def test_flush_many_new(self):
    interface.state.global_monitor = stubs.MockMonitor()
    interface.state.target = targets.TaskTarget('a', 'b', 'c', 'd', 1)
    interface.state.use_new_proto = True

    # We can't use the mock's call_args_list here because the same object is
    # reused as the argument to both calls and cleared inbetween.
    data_lengths = []
    def send(proto):
      count = 0
      for coll in proto.metrics_collection:
        for data_set in coll.metrics_data_set:
          for _ in data_set.data:
            count += 1
      data_lengths.append(count)
    interface.state.global_monitor.send.side_effect = send

    counter = metrics.CounterMetric('counter', description='desc')
    interface.register(counter)

    for i in xrange(interface.METRICS_DATA_LENGTH_LIMIT + 1):
      counter.increment_by(i, {'field': i})

    interface.flush()
    self.assertEquals(2, interface.state.global_monitor.send.call_count)
    self.assertListEqual([500, 1], data_lengths)

  def test_flush_different_target_fields(self):
    interface.state.global_monitor = stubs.MockMonitor()
    interface.state.target = targets.TaskTarget('s', 'j', 'r', 'h')
    metric = metrics.GaugeMetric('m')

    metric.set(123)
    metric.set(456, target_fields={'service_name': 'foo'})
    interface.flush()

    interface.state.global_monitor.send.assert_called_once()
    proto = interface.state.global_monitor.send.call_args[0][0]
    self.assertEqual(2, len(proto.data))
    self.assertEqual(123, proto.data[0].gauge)
    self.assertEqual(456, proto.data[1].gauge)
    self.assertEqual('s', proto.data[0].task.service_name)
    self.assertEqual('foo', proto.data[1].task.service_name)

  def test_flush_different_target_fields_new(self):
    interface.state.metric_name_prefix = '/infra/test/'
    interface.state.global_monitor = stubs.MockMonitor()
    interface.state.target = targets.TaskTarget('s', 'j', 'r', 'h')
    interface.state.use_new_proto = True
    metric = metrics.GaugeMetric('m')

    metric.set(123)
    metric.set(456, target_fields={'service_name': 'foo'})
    interface.flush()

    interface.state.global_monitor.send.assert_called_once()
    proto = interface.state.global_monitor.send.call_args[0][0]
    col = proto.metrics_collection
    self.assertEqual(2, len(col))
    self.assertEqual(123, col[0].metrics_data_set[0].data[0].int64_value)
    self.assertEqual(456, col[1].metrics_data_set[0].data[0].int64_value)
    self.assertEqual('s', col[0].task.service_name)
    self.assertEqual('foo', col[1].task.service_name)

  def test_send_modifies_metric_values(self):
    interface.state.global_monitor = stubs.MockMonitor()
    interface.state.target = stubs.MockTarget()

    # pylint: disable=unused-argument
    def serialize_to(pb, start_time, fields, value, target):
      pb.name = 'foo'

    fake_metric = mock.create_autospec(metrics.Metric, spec_set=True)
    fake_metric.name = 'fake'
    fake_metric.serialize_to.side_effect = serialize_to
    interface.register(fake_metric)

    # Setting this will modify store._values in the middle of iteration.
    delayed_metric = metrics.CounterMetric('foo')
    def send(proto):
      delayed_metric.increment_by(1)
    interface.state.global_monitor.send.side_effect = send

    for i in xrange(1001):
      interface.state.store.set('fake', ('field', i), None, 123)

    # Shouldn't raise an exception.
    interface.flush()

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
    self.assertEqual(1, interface.flush.call_count)

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
    self.assertEqual(2, interface.flush.call_count)

  def test_stop_stops(self):
    self.t.start()

    self.assertTrue(self.t.is_alive())

    self.t.stop()
    self.assertFalse(self.t.is_alive())

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


class GenerateNewProtoTest(unittest.TestCase):
  """Test _generate_proto_new()."""

  def setUp(self):
    interface.state = interface.State()
    interface.state.use_new_proto = True
    interface.state.metric_name_prefix = '/infra/test/'
    interface.state.target = targets.TaskTarget(
        service_name='service', job_name='job', region='region',
        hostname='hostname', task_num=0)

    self.time_fn = mock.create_autospec(time.time, spec_set=True)
    interface.state.store = metric_store.InProcessMetricStore(
        interface.state, self.time_fn)

  def test_grouping(self):
    counter0 = metrics.CounterMetric('counter0', description='desc0')
    counter1 = metrics.CounterMetric('counter1', description='desc1')
    counter2 = metrics.CounterMetric('counter2', description='desc2')

    interface.register(counter0)
    interface.register(counter1)
    interface.register(counter2)

    counter0.increment_by(3, fields={'test': 123})
    counter0.increment_by(5, fields={'test': 999})
    counter1.increment()
    counter2.increment_by(4, fields={}, target_fields={'task_num': 1})

    protos = list(interface._generate_proto_new())
    self.assertEqual(1, len(protos))

    proto = protos[0]
    self.assertEqual(2, len(proto.metrics_collection))

    for coll in proto.metrics_collection:
      self.assertEqual('service', coll.task.service_name)
      self.assertEqual('job', coll.task.job_name)
      self.assertEqual('region', coll.task.data_center)
      self.assertEqual('hostname', coll.task.host_name)

    first_coll = proto.metrics_collection[0]
    second_coll = proto.metrics_collection[1]

    self.assertEqual(0, first_coll.task.task_num)
    self.assertEqual(1, second_coll.task.task_num)

    self.assertEqual(2, len(first_coll.metrics_data_set))
    self.assertEqual(1, len(second_coll.metrics_data_set))

    data_sets = [
        first_coll.metrics_data_set[0],
        first_coll.metrics_data_set[1],
        second_coll.metrics_data_set[0]
    ]

    for i, data_set in enumerate(data_sets):
      self.assertEqual('/infra/test/counter%d' % i, data_set.metric_name)

  def test_generate_every_type_of_field(self):
    counter = metrics.CounterMetric('counter')
    interface.register(counter)
    counter.increment({'a': 1, 'b': True, 'c': 'test'})

    proto = list(interface._generate_proto_new())[0]
    data_set = proto.metrics_collection[0].metrics_data_set[0]

    field_type = new_metrics_pb2.MetricsDataSet.MetricFieldDescriptor
    self.assertEqual('a', data_set.field_descriptor[0].name)
    self.assertEqual(field_type.INT64, data_set.field_descriptor[0].field_type)

    self.assertEqual('b', data_set.field_descriptor[1].name)
    self.assertEqual(field_type.BOOL, data_set.field_descriptor[1].field_type)

    self.assertEqual('c', data_set.field_descriptor[2].name)
    self.assertEqual(field_type.STRING,
                     data_set.field_descriptor[2].field_type)

    self.assertEqual(1, data_set.data[0].int64_value)

    self.assertEqual('a', data_set.data[0].field[0].name)
    self.assertEqual(1, data_set.data[0].field[0].int64_value)

    self.assertEqual('b', data_set.data[0].field[1].name)
    self.assertTrue(data_set.data[0].field[1].bool_value)

    self.assertEqual('c', data_set.data[0].field[2].name)
    self.assertEqual('test', data_set.data[0].field[2].string_value)

