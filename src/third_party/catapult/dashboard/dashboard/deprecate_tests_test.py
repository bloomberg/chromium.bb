# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function
from __future__ import division
from __future__ import absolute_import

import datetime
import mock
import unittest

import webapp2
import webtest

from google.appengine.ext import ndb

from dashboard import deprecate_tests
from dashboard.common import testing_common
from dashboard.common import utils


_DEPRECATE_DAYS = deprecate_tests._DEPRECATION_REVISION_DELTA.days + 1

_REMOVAL_DAYS = deprecate_tests._REMOVAL_REVISON_DELTA.days + 1

_TESTS_SIMPLE = [
    ['ChromiumPerf'],
    ['mac'],
    {
        'SunSpider': {
            'Total': {
                't': {},
                't_ref': {},
            },
        }
    }
]

_TESTS_MULTIPLE = [
    ['ChromiumPerf'],
    ['mac'],
    {
        'SunSpider': {
            'Total': {
                't': {},
                't_ref': {},
            },
        },
        'OtherTest': {
            'OtherMetric': {
                'foo1': {},
                'foo2': {},
            },
        },
    }
]

_TESTS_MULTIPLE_MASTERS_AND_BOTS = [
    ['ChromiumPerf', 'ChromiumPerfFYI'],
    ['mac', 'linux'],
    {
        'SunSpider': {
            'Total': {
                't': {},
            },
        }
    }
]


class DeprecateTestsTest(testing_common.TestCase):

  def setUp(self):
    super(DeprecateTestsTest, self).setUp()
    app = webapp2.WSGIApplication([(
        '/deprecate_tests', deprecate_tests.DeprecateTestsHandler)])
    self.testapp = webtest.TestApp(app)

    deprecate_tests._DEPRECATE_TESTS_PARALLEL_SHARDS = 2

  def _AddMockRows(self, test_path, age):
    """Adds sample TestMetadata and Row entities."""

    # Add 50 Row entities to some of the tests.
    ts = datetime.datetime.now() - datetime.timedelta(days=age)
    data = {}
    for i in range(15000, 15100, 2):
      data[i] = {'value': 1, 'timestamp': ts}

    testing_common.AddRows(test_path, data)

  def AssertDeprecated(self, test_path, deprecated):
    test_key = utils.TestKey(test_path)
    test = test_key.get()

    self.assertEqual(test.deprecated, deprecated)

  @mock.patch.object(deprecate_tests, '_AddDeleteTestDataTask')
  def testPost_DeprecateOldTest(self, mock_delete):
    testing_common.AddTests(*_TESTS_MULTIPLE)
    self._AddMockRows('ChromiumPerf/mac/SunSpider/Total/t', _DEPRECATE_DAYS)
    self._AddMockRows('ChromiumPerf/mac/SunSpider/Total/t_ref', 0)
    self._AddMockRows('ChromiumPerf/mac/OtherTest/OtherMetric/foo1', 0)
    self._AddMockRows('ChromiumPerf/mac/OtherTest/OtherMetric/foo2', 0)
    self.testapp.post('/deprecate_tests')
    self.ExecuteTaskQueueTasks(
        '/deprecate_tests', deprecate_tests._DEPRECATE_TESTS_TASK_QUEUE_NAME)

    self.AssertDeprecated('ChromiumPerf/mac/SunSpider', False)
    self.AssertDeprecated('ChromiumPerf/mac/SunSpider/Total/t', True)
    self.AssertDeprecated('ChromiumPerf/mac/SunSpider/Total/t_ref', False)

    self.assertFalse(mock_delete.called)

  @mock.patch.object(deprecate_tests, '_AddDeleteTestDataTask')
  def testPost_DeprecateOldTestDeprecatesSuite(self, mock_delete):
    testing_common.AddTests(*_TESTS_MULTIPLE)
    self._AddMockRows('ChromiumPerf/mac/SunSpider/Total/t', _DEPRECATE_DAYS)
    self._AddMockRows('ChromiumPerf/mac/SunSpider/Total/t_ref', _DEPRECATE_DAYS)
    self._AddMockRows('ChromiumPerf/mac/OtherTest/OtherMetric/foo1', 0)
    self._AddMockRows('ChromiumPerf/mac/OtherTest/OtherMetric/foo2', 0)
    self.testapp.post('/deprecate_tests')
    self.ExecuteTaskQueueTasks(
        '/deprecate_tests', deprecate_tests._DEPRECATE_TESTS_TASK_QUEUE_NAME)

    # Do a second pass to catch the suite
    self.testapp.post('/deprecate_tests')
    self.ExecuteTaskQueueTasks(
        '/deprecate_tests', deprecate_tests._DEPRECATE_TESTS_TASK_QUEUE_NAME)

    self.AssertDeprecated('ChromiumPerf/mac/SunSpider', True)
    self.AssertDeprecated('ChromiumPerf/mac/SunSpider/Total/t', True)
    self.AssertDeprecated('ChromiumPerf/mac/SunSpider/Total/t_ref', True)

    self.assertFalse(mock_delete.called)

  @mock.patch.object(deprecate_tests, '_AddDeleteTestDataTask')
  def testPost_DoesNotDeleteRowsWithChildren(self, mock_delete):
    testing_common.AddTests(*_TESTS_SIMPLE)
    self._AddMockRows('ChromiumPerf/mac/SunSpider/Total', _REMOVAL_DAYS)
    self._AddMockRows('ChromiumPerf/mac/SunSpider/Total/t', 0)
    self._AddMockRows('ChromiumPerf/mac/SunSpider/Total/t_ref', 0)
    self.testapp.post('/deprecate_tests')
    self.ExecuteTaskQueueTasks(
        '/deprecate_tests', deprecate_tests._DEPRECATE_TESTS_TASK_QUEUE_NAME)

    # Do a second pass to catch the suite
    self.testapp.post('/deprecate_tests')
    self.ExecuteTaskQueueTasks(
        '/deprecate_tests', deprecate_tests._DEPRECATE_TESTS_TASK_QUEUE_NAME)

    self.AssertDeprecated('ChromiumPerf/mac/SunSpider', False)
    self.AssertDeprecated('ChromiumPerf/mac/SunSpider/Total', True)
    self.AssertDeprecated('ChromiumPerf/mac/SunSpider/Total/t', False)

    self.assertFalse(mock_delete.called)

  @mock.patch.object(deprecate_tests, '_AddDeleteTestDataTask')
  def testPost_DeprecateOldTestDeletesData(self, mock_delete):
    testing_common.AddTests(*_TESTS_MULTIPLE)
    self._AddMockRows('ChromiumPerf/mac/SunSpider/Total/t', _REMOVAL_DAYS)
    self._AddMockRows('ChromiumPerf/mac/SunSpider/Total/t_ref', 0)
    self._AddMockRows('ChromiumPerf/mac/OtherTest/OtherMetric/foo1', 0)
    self._AddMockRows('ChromiumPerf/mac/OtherTest/OtherMetric/foo2', 0)
    self.testapp.post('/deprecate_tests')
    self.ExecuteTaskQueueTasks(
        '/deprecate_tests', deprecate_tests._DEPRECATE_TESTS_TASK_QUEUE_NAME)

    test = utils.TestKey('ChromiumPerf/mac/SunSpider/Total/t').get()
    mock_delete.assert_called_once_with(test)

  @mock.patch.object(deprecate_tests, '_AddDeleteTestDataTask')
  def testPost_DeletesTestsWithNoRowsOrChildren(self, mock_delete):
    testing_common.AddTests(*_TESTS_MULTIPLE)
    self._AddMockRows('ChromiumPerf/mac/SunSpider/Total/t', 0)
    self._AddMockRows('ChromiumPerf/mac/OtherTest/OtherMetric/foo1', 0)
    self._AddMockRows('ChromiumPerf/mac/OtherTest/OtherMetric/foo2', 0)
    self.testapp.post('/deprecate_tests')
    self.ExecuteTaskQueueTasks(
        '/deprecate_tests', deprecate_tests._DEPRECATE_TESTS_TASK_QUEUE_NAME)

    test = utils.TestKey('ChromiumPerf/mac/SunSpider/Total/t_ref').get()
    mock_delete.assert_called_once_with(test)

  @mock.patch.object(
      deprecate_tests, '_AddDeprecateTestDataTask', mock.MagicMock())
  def testPost_DeletesBot_NotMaster(self):
    testing_common.AddTests(*_TESTS_MULTIPLE_MASTERS_AND_BOTS)
    utils.TestKey('ChromiumPerf/mac/SunSpider/Total/t').delete()
    utils.TestKey('ChromiumPerf/mac/SunSpider/Total').delete()
    utils.TestKey('ChromiumPerf/mac/SunSpider').delete()

    for m in _TESTS_MULTIPLE_MASTERS_AND_BOTS[0]:
      for b in _TESTS_MULTIPLE_MASTERS_AND_BOTS[1]:
        master_key = ndb.Key('Master', m)
        bot_key = ndb.Key('Bot', b, parent=master_key)
        self.assertIsNotNone(bot_key.get())
        self.assertIsNotNone(master_key.get())

    self.testapp.get('/deprecate_tests')

    self.ExecuteDeferredTasks(deprecate_tests._DEPRECATE_TESTS_TASK_QUEUE_NAME)

    expected_deleted_bots = [ndb.Key('Master', 'ChromiumPerf', 'Bot', 'mac')]
    for m in _TESTS_MULTIPLE_MASTERS_AND_BOTS[0]:
      for b in _TESTS_MULTIPLE_MASTERS_AND_BOTS[1]:
        master_key = ndb.Key('Master', m)
        bot_key = ndb.Key('Bot', b, parent=master_key)
        if bot_key in expected_deleted_bots:
          self.assertIsNone(bot_key.get())
        else:
          self.assertIsNotNone(bot_key.get())
        self.assertIsNotNone(master_key.get())

  @mock.patch.object(
      deprecate_tests, '_AddDeprecateTestDataTask', mock.MagicMock())
  def testPost_DeletesMasterAndBot(self):
    testing_common.AddTests(*_TESTS_MULTIPLE_MASTERS_AND_BOTS)
    utils.TestKey('ChromiumPerf/mac/SunSpider/Total/t').delete()
    utils.TestKey('ChromiumPerf/mac/SunSpider/Total').delete()
    utils.TestKey('ChromiumPerf/mac/SunSpider').delete()
    utils.TestKey('ChromiumPerf/linux/SunSpider/Total/t').delete()
    utils.TestKey('ChromiumPerf/linux/SunSpider/Total').delete()
    utils.TestKey('ChromiumPerf/linux/SunSpider').delete()

    for m in _TESTS_MULTIPLE_MASTERS_AND_BOTS[0]:
      for b in _TESTS_MULTIPLE_MASTERS_AND_BOTS[1]:
        master_key = ndb.Key('Master', m)
        bot_key = ndb.Key('Bot', b, parent=master_key)
        self.assertIsNotNone(bot_key.get())
        self.assertIsNotNone(master_key.get())

    self.testapp.get('/deprecate_tests')

    self.ExecuteDeferredTasks(deprecate_tests._DEPRECATE_TESTS_TASK_QUEUE_NAME)

    expected_deleted_bots = [
        ndb.Key('Master', 'ChromiumPerf', 'Bot', 'mac'),
        ndb.Key('Master', 'ChromiumPerf', 'Bot', 'linux')]
    expected_deleted_masters = [ndb.Key('Master', 'ChromiumPerf')]
    for m in _TESTS_MULTIPLE_MASTERS_AND_BOTS[0]:
      master_key = ndb.Key('Master', m)
      if master_key in expected_deleted_masters:
        self.assertIsNone(master_key.get())
      else:
        self.assertIsNotNone(master_key.get())

      for b in _TESTS_MULTIPLE_MASTERS_AND_BOTS[1]:
        bot_key = ndb.Key('Bot', b, parent=master_key)
        if bot_key in expected_deleted_bots:
          self.assertIsNone(bot_key.get())
        else:
          self.assertIsNotNone(bot_key.get())


if __name__ == '__main__':
  unittest.main()
