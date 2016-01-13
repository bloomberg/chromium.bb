# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import mock

from infra_libs.ts_mon.common import metrics
from infra_libs.ts_mon.common import helpers


class ScopedIncrementCounterTest(unittest.TestCase):

  def test_success(self):
    counter = mock.Mock(metrics.CounterMetric)
    with helpers.ScopedIncrementCounter(counter):
      pass
    counter.increment.assert_called_once_with({'status': 'success'})

  def test_exception(self):
    counter = mock.Mock(metrics.CounterMetric)
    with self.assertRaises(Exception):
      with helpers.ScopedIncrementCounter(counter):
        raise Exception()
    counter.increment.assert_called_once_with({'status': 'failure'})

  def test_custom_status(self):
    counter = mock.Mock(metrics.CounterMetric)
    with helpers.ScopedIncrementCounter(counter) as sc:
      sc.set_status('foo')
    counter.increment.assert_called_once_with({'status': 'foo'})

  def test_set_failure(self):
    counter = mock.Mock(metrics.CounterMetric)
    with helpers.ScopedIncrementCounter(counter) as sc:
      sc.set_failure()
    counter.increment.assert_called_once_with({'status': 'failure'})

  def test_custom_status_and_exception(self):
    counter = mock.Mock(metrics.CounterMetric)
    with self.assertRaises(Exception):
      with helpers.ScopedIncrementCounter(counter) as sc:
        sc.set_status('foo')
        raise Exception()
    counter.increment.assert_called_once_with({'status': 'foo'})

  def test_multiple_custom_status_calls(self):
    counter = mock.Mock(metrics.CounterMetric)
    with helpers.ScopedIncrementCounter(counter) as sc:
      sc.set_status('foo')
      sc.set_status('bar')
    counter.increment.assert_called_once_with({'status': 'bar'})

  def test_custom_label_success(self):
    counter = mock.Mock(metrics.CounterMetric)
    with helpers.ScopedIncrementCounter(counter, 'a', 'b', 'c'):
      pass
    counter.increment.assert_called_once_with({'a': 'b'})

  def test_custom_label_exception(self):
    counter = mock.Mock(metrics.CounterMetric)
    with self.assertRaises(Exception):
      with helpers.ScopedIncrementCounter(counter, 'a', 'b', 'c'):
        raise Exception()
    counter.increment.assert_called_once_with({'a': 'c'})
