# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import unittest

import mock

from core import results_dashboard


class ResultsDashboardTest(unittest.TestCase):

  def setUp(self):
    self.fake_service = '/foo/bar/kingsman-service-account'
    self.dummy_token_generator = lambda service_file, timeout: 'Arthur-Merlin'
    self.perf_data = {'foo': 1, 'bar': 2}
    self.dashboard_url = 'https://chromeperf.appspot.com'

  def testRetryForSendResultRetryException(self):
    def raise_retry_exception(url, histogramset_json, service_account_file):
      del url, histogramset_json, service_account_file  # unused
      raise results_dashboard.SendResultsRetryException('Should retry')

    with mock.patch('core.results_dashboard._SendHistogramJson',
                    side_effect=raise_retry_exception) as m:
      upload_result = results_dashboard.SendResults(
          self.perf_data, self.dashboard_url, send_as_histograms=True,
          service_account_file=self.fake_service,
          token_generator_callback=self.dummy_token_generator, num_retries=5)
      self.assertFalse(upload_result)
      self.assertEqual(m.call_count, 5)

  def testNoRetryForSendResultFatalException(self):

    def raise_retry_exception(url, histogramset_json, service_account_file):
      del url, histogramset_json, service_account_file  # unused
      raise results_dashboard.SendResultsFatalException('Do not retry')

    with mock.patch('core.results_dashboard._SendHistogramJson',
                    side_effect=raise_retry_exception) as m:
      upload_result =  results_dashboard.SendResults(
          self.perf_data, self.dashboard_url, send_as_histograms=True,
          service_account_file=self.fake_service,
          token_generator_callback=self.dummy_token_generator,
          num_retries=5)
      self.assertFalse(upload_result)
      self.assertEqual(m.call_count, 1)

  def testNoRetryForSuccessfulSendResult(self):
    with mock.patch('core.results_dashboard._SendHistogramJson') as m:
      upload_result = results_dashboard.SendResults(
          self.perf_data, self.dashboard_url, send_as_histograms=True,
          service_account_file=self.fake_service,
          token_generator_callback=self.dummy_token_generator,
          num_retries=5)
      self.assertTrue(upload_result)
      self.assertEqual(m.call_count, 1)

  def testNoRetryAfterSucessfulSendResult(self):
    counter = [0]
    def raise_retry_exception_first_two_times(
        url, histogramset_json, service_account_file):
      del url, histogramset_json, service_account_file  # unused
      counter[0] += 1
      if counter[0] <= 2:
        raise results_dashboard.SendResultsRetryException('Please retry')

    with mock.patch('core.results_dashboard._SendHistogramJson',
                    side_effect=raise_retry_exception_first_two_times) as m:
      upload_result = results_dashboard.SendResults(
          self.perf_data, self.dashboard_url, send_as_histograms=True,
          service_account_file=self.fake_service,
          token_generator_callback=self.dummy_token_generator,
          num_retries=5)
      self.assertTrue(upload_result)
      self.assertEqual(m.call_count, 3)
