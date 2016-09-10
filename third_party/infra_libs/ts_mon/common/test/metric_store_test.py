# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import functools
import operator
import time
import unittest

import mock

from infra_libs.ts_mon.common import errors
from infra_libs.ts_mon.common import metric_store
from infra_libs.ts_mon.common import metrics
from infra_libs.ts_mon.common.test import stubs


class DefaultModifyFnTest(unittest.TestCase):
  def test_adds(self):
    fn = metric_store.default_modify_fn('foo')
    self.assertEquals(5, fn(2, 3))
    self.assertEquals(5, fn(3, 2))

  def test_negative(self):
    fn = metric_store.default_modify_fn('foo')
    with self.assertRaises(errors.MonitoringDecreasingValueError) as cm:
      fn(2, -1)
    self.assertIn('"foo"', str(cm.exception))


class MetricStoreTestBase(object):
  """Abstract base class for testing MetricStore implementations.

  This class doesn't inherit from unittest.TestCase to prevent it from being
  run automatically by expect_tests.

  Your subclass should inherit from this and unittest.TestCase, and set
  METRIC_STORE_CLASS to the implementation you want to test.  See
  InProcessMetricStoreTest in this file for an example.
  """

  METRIC_STORE_CLASS = None

  def setUp(self):
    super(MetricStoreTestBase, self).setUp()

    self.mock_time = mock.create_autospec(time.time, spec_set=True)
    self.mock_time.return_value = 1234

    self.state = stubs.MockState(store_ctor=self.create_store)
    mock.patch('infra_libs.ts_mon.common.interface.state',
        new=self.state).start()

    self.store = self.state.store

    self.metric = metrics.Metric('foo')

  def create_store(self, *args, **kwargs):
    kwargs['time_fn'] = self.mock_time
    return self.METRIC_STORE_CLASS(*args, **kwargs)

  def tearDown(self):
    super(MetricStoreTestBase, self).tearDown()

    mock.patch.stopall()

  def test_sets_start_time(self):
    self.metric._start_time = None
    self.mock_time.return_value = 1234

    self.store.set('foo', (('field', 'value'),), 42)
    self.store.set('foo', (('field', 'value2'),), 43)

    all_metrics = list(self.store.get_all())
    self.assertEqual(1, len(all_metrics))
    self.assertEqual('foo', all_metrics[0][1].name)
    self.assertEqual(1234, all_metrics[0][2])

    self.mock_time.assert_called_once_with()

  def test_uses_start_time_from_metric(self):
    self.metric._start_time = 5678

    self.store.set('foo', (('field', 'value'),), 42)
    self.store.set('foo', (('field', 'value2'),), 43)

    all_metrics = list(self.store.get_all())
    self.assertEqual(1, len(all_metrics))
    self.assertEqual('foo', all_metrics[0][1].name)
    self.assertEqual(5678, all_metrics[0][2])

    self.assertFalse(self.mock_time.called)

  def test_get(self):
    self.store.set('foo', (('field', 'value'),), 42)
    self.store.set('foo', (('field', 'value2'),), 43)

    self.assertEquals(42, self.store.get('foo', (('field', 'value'),)))
    self.assertEquals(43, self.store.get('foo', (('field', 'value2'),)))
    self.assertIsNone(self.store.get('foo', (('field', 'value3'),)))
    self.assertIsNone(self.store.get('foo', ()))
    self.assertEquals(44, self.store.get('foo', (('field', 'value3'),),
                                         default=44))

    self.assertIsNone(self.store.get('bar', ()))

  def test_set_enforce_ge(self):
    self.store.set('foo', (('field', 'value'),), 42, enforce_ge=True)
    self.store.set('foo', (('field', 'value'),), 43, enforce_ge=True)

    with self.assertRaises(errors.MonitoringDecreasingValueError):
      self.store.set('foo', (('field', 'value'),), 42, enforce_ge=True)

  def test_incr(self):
    self.store.set('foo', (('field', 'value'),), 42)
    self.store.incr('foo', (('field', 'value'),), 4)

    self.assertEquals(46, self.store.get('foo', (('field', 'value'),)))

    with self.assertRaises(errors.MonitoringDecreasingValueError):
      self.store.incr('foo', (('field', 'value'),), -1)

  def test_incr_modify_fn(self):
    modify_fn = mock.Mock()
    modify_fn.return_value = 7

    self.store.set('foo', (('field', 'value'),), 42)
    self.store.incr('foo', (('field', 'value'),), 3, modify_fn=modify_fn)

    self.assertEquals(7, self.store.get('foo', (('field', 'value'),)))
    modify_fn.assert_called_once_with(42, 3)

  def test_reset_for_unittest(self):
    self.store.set('foo', (('field', 'value'),), 42)
    self.store.reset_for_unittest()
    self.assertIsNone(self.store.get('foo', (('field', 'value'),)))

  def test_reset_for_unittest_name(self):
    self.store.set('foo', (('field', 'value'),), 42)
    self.store.reset_for_unittest(name='bar')
    self.assertEquals(42, self.store.get('foo', (('field', 'value'),)))

    self.store.reset_for_unittest(name='foo')
    self.assertIsNone(self.store.get('foo', (('field', 'value'),)))


class InProcessMetricStoreTest(MetricStoreTestBase, unittest.TestCase):
  METRIC_STORE_CLASS = metric_store.InProcessMetricStore
