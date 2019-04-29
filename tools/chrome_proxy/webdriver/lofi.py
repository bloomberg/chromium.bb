# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import common
from common import TestDriver
from common import IntegrationTest
from decorators import ChromeVersionBeforeM
from decorators import ChromeVersionEqualOrAfterM

class LoFi(IntegrationTest):

  # Checks that LoFi images are served when LoFi slow connections are used and
  # the network quality estimator returns Slow2G.
  @ChromeVersionEqualOrAfterM(65)
  @ChromeVersionBeforeM(75)
  def testLoFiOnSlowConnection(self):
    with TestDriver() as test_driver:
      test_driver.AddChromeArg('--enable-spdy-proxy-auth')
      test_driver.AddChromeArg('--enable-features='
                               'Previews,DataReductionProxyDecidesTransform')
      # Disable server experiments such as tamper detection.
      test_driver.AddChromeArg('--data-reduction-proxy-server-experiments-'
                               'disabled')
      test_driver.AddChromeArg('--force-fieldtrial-params='
                               'NetworkQualityEstimator.Enabled:'
                               'force_effective_connection_type/Slow2G')
      test_driver.AddChromeArg('--force-fieldtrials=NetworkQualityEstimator/'
                               'Enabled')

      test_driver.LoadURL('http://check.googlezip.net/static/index.html')

      lofi_responses = 0
      for response in test_driver.GetHTTPResponses():
        if not response.url.endswith('png'):
          continue
        if not response.request_headers:
          continue
        if (self.checkLoFiResponse(response, True)):
          lofi_responses = lofi_responses + 1

      # Verify that Lo-Fi responses were seen.
      self.assertNotEqual(0, lofi_responses)

      self.assertPreviewShownViaHistogram(test_driver, 'LoFi')

  # Checks that LoFi images are NOT served when the network quality estimator
  # returns fast connection.
  @ChromeVersionEqualOrAfterM(65)
  @ChromeVersionBeforeM(75)
  def testLoFiFastConnection(self):
    with TestDriver() as test_driver:
      test_driver.AddChromeArg('--enable-spdy-proxy-auth')
      test_driver.AddChromeArg('--enable-features='
                               'Previews,DataReductionProxyDecidesTransform')
      # Disable server experiments such as tamper detection.
      test_driver.AddChromeArg('--data-reduction-proxy-server-experiments-'
                               'disabled')
      test_driver.AddChromeArg('--force-fieldtrial-params='
                               'NetworkQualityEstimator.Enabled:'
                               'force_effective_connection_type/4G')
      test_driver.AddChromeArg('--force-fieldtrials=NetworkQualityEstimator/'
                               'Enabled')

      test_driver.LoadURL('http://check.googlezip.net/static/index.html')

      lofi_responses = 0
      for response in test_driver.GetHTTPResponses():
        if response.url.endswith('html'):
          # Main resource should accept transforms but not be transformed.
          self.assertEqual('lite-page',
            response.request_headers['chrome-proxy-accept-transform'])
          self.assertNotIn('chrome-proxy-content-transform',
            response.response_headers)
          if 'chrome-proxy' in response.response_headers:
            self.assertNotIn('page-policies',
                             response.response_headers['chrome-proxy'])
        else:
          # No subresources should accept transforms.
          self.assertNotIn('chrome-proxy-accept-transform',
            response.request_headers)

      self.assertPreviewNotShownViaHistogram(test_driver, 'LoFi')

  # Checks that Lo-Fi placeholder images are not loaded from cache on page
  # reloads when Lo-Fi mode is disabled or data reduction proxy is disabled.
  # First a test page is opened with Lo-Fi and chrome proxy enabled. This allows
  # Chrome to cache the Lo-Fi placeholder image. The browser is restarted with
  # chrome proxy disabled and the same test page is loaded. This second page
  # load should not pick the Lo-Fi placeholder from cache and original image
  # should be loaded. Finally, the browser is restarted with chrome proxy
  # enabled and Lo-Fi disabled and the same test page is loaded. This third page
  # load should not pick the Lo-Fi placeholder from cache and original image
  # should be loaded.
  @ChromeVersionEqualOrAfterM(65)
  @ChromeVersionBeforeM(75)
  def testLoFiCacheBypass(self):
    # If it was attempted to run with another experiment, skip this test.
    if common.ParseFlags().browser_args and ('--data-reduction-proxy-experiment'
        in common.ParseFlags().browser_args):
      self.skipTest('This test cannot be run with other experiments.')
    with TestDriver() as test_driver:
      # First page load, enable Lo-Fi and chrome proxy. Disable server
      # experiments such as tamper detection. This test should be run with
      # --profile-type=default command line for the same user profile and cache
      # to be used across the two page loads.
      test_driver.AddChromeArg('--enable-spdy-proxy-auth')
      test_driver.AddChromeArg('--enable-features='
                               'Previews,DataReductionProxyDecidesTransform')
      test_driver.AddChromeArg('--profile-type=default')
      test_driver.AddChromeArg('--data-reduction-proxy-server-experiments-'
                               'disabled')
      test_driver.AddChromeArg('--force-fieldtrial-params='
                               'NetworkQualityEstimator.Enabled:'
                               'force_effective_connection_type/Slow2G')
      test_driver.AddChromeArg('--force-fieldtrials=NetworkQualityEstimator/'
                               'Enabled')

      test_driver.LoadURL('http://check.googlezip.net/cacheable/test.html')

      lofi_responses = 0
      for response in test_driver.GetHTTPResponses():
        if not response.url.endswith('png'):
          continue
        if not response.request_headers:
          continue
        if (self.checkLoFiResponse(response, True)):
          lofi_responses = lofi_responses + 1

      # Verify that Lo-Fi responses were seen.
      self.assertNotEqual(0, lofi_responses)

      # Second page load with the chrome proxy off.
      test_driver._StopDriver()
      test_driver.RemoveChromeArg('--enable-spdy-proxy-auth')
      test_driver.LoadURL('http://check.googlezip.net/cacheable/test.html')

      responses = 0
      for response in test_driver.GetHTTPResponses():
        if not response.url.endswith('png'):
          continue
        if not response.request_headers:
          continue
        responses = responses + 1
        self.assertNotHasChromeProxyViaHeader(response)
        self.checkLoFiResponse(response, False)

      # Verify that responses were seen.
      self.assertNotEqual(0, responses)

      # Third page load with the chrome proxy on and Lo-Fi off.
      test_driver._StopDriver()
      test_driver.AddChromeArg('--enable-spdy-proxy-auth')
      test_driver.RemoveChromeArg('--enable-features='
                                  'DataReductionProxyDecidesTransform')
      test_driver.AddChromeArg('--disable-features='
                               'DataReductionProxyDecidesTransform')
      test_driver.LoadURL('http://check.googlezip.net/cacheable/test.html')

      responses = 0
      for response in test_driver.GetHTTPResponses():
        if not response.url.endswith('png'):
          continue
        if not response.request_headers:
          continue
        responses = responses + 1
        self.assertHasProxyHeaders(response)
        self.checkLoFiResponse(response, False)

      # Verify that responses were seen.
      self.assertNotEqual(0, responses)

  # Checks that Client LoFi resource requests have the Intervention header.
  @ChromeVersionEqualOrAfterM(61)
  @ChromeVersionBeforeM(75)
  def testClientLoFiInterventionHeader(self):
    with TestDriver() as test_driver:
      test_driver.AddChromeArg('--enable-spdy-proxy-auth')
      test_driver.AddChromeArg('--enable-features='
                               'Previews,DataReductionProxyDecidesTransform')
      test_driver.AddChromeArg(
          '--force-fieldtrial-params=NetworkQualityEstimator.Enabled:'
          'force_effective_connection_type/2G,'
          'PreviewsClientLoFi.Enabled:'
          'max_allowed_effective_connection_type/4G')
      test_driver.AddChromeArg(
          '--force-fieldtrials=NetworkQualityEstimator/Enabled/'
          'PreviewsClientLoFi/Enabled')

      test_driver.LoadURL('https://check.googlezip.net/static/index.html')

      intervention_headers = 0
      for response in test_driver.GetHTTPResponses():
        if not response.url.endswith('html'):
          self.assertIn('intervention', response.request_headers)
          intervention_headers = intervention_headers + 1

      self.assertNotEqual(0, intervention_headers)

  @ChromeVersionEqualOrAfterM(75)
  def testServerLoFiWithForcingFlag(self):
    with TestDriver() as test_driver:
      test_driver.AddChromeArg('--enable-spdy-proxy-auth')
      test_driver.AddChromeArg('--enable-features=' + ','.join([
                                 'Previews',
                                 'DataReductionProxyDecidesTransform',
                                 'DataReductionProxyEnabledWithNetworkService',
                               ]))
      test_driver.AddChromeArg('--force-effective-connection-type=Slow-2G')
      test_driver.SetExperiment('force_page_policies_empty_image')

      test_driver.LoadURL('http://check.googlezip.net/static/index.html')

      responses = test_driver.GetHTTPResponses()
      self.assertNotEqual(len(responses), 0)
      for response in responses:
        if response.url.endswith('html'):
          self.assertIn('empty-image',
            response.response_headers['chrome-proxy'])

  @ChromeVersionEqualOrAfterM(74)
  def testNoServerLoFiByDefault(self):
    with TestDriver() as test_driver:
      test_driver.AddChromeArg('--enable-spdy-proxy-auth')
      test_driver.AddChromeArg('--enable-features=' + ','.join([
                                 'Previews',
                                 'DataReductionProxyDecidesTransform',
                                 'DataReductionProxyEnabledWithNetworkService',
                               ]))
      test_driver.AddChromeArg('--force-effective-connection-type=Slow-2G')

      test_driver.LoadURL('http://check.googlezip.net/static/index.html')

      responses = test_driver.GetHTTPResponses()
      self.assertNotEqual(len(responses), 0)
      for response in responses:
        if response.url.endswith('html'):
          self.assertNotIn('empty-image',
            response.response_headers['chrome-proxy'])

  # Checks that Client LoFi range requests that go through the Data Reduction
  # Proxy are returned correctly.
  @ChromeVersionEqualOrAfterM(62)
  @ChromeVersionBeforeM(75)
  def testClientLoFiRangeRequestThroughDataReductionProxy(self):
    with TestDriver() as test_driver:
      test_driver.AddChromeArg('--enable-spdy-proxy-auth')
      # Enable Previews and Client-side LoFi, but disable server previews in
      # order to force Chrome to use Client-side LoFi for the images on the
      # page.
      test_driver.AddChromeArg('--enable-features=Previews,PreviewsClientLoFi')
      test_driver.AddChromeArg(
          '--disable-features=DataReductionProxyDecidesTransform')

      test_driver.AddChromeArg(
          '--force-fieldtrial-params=NetworkQualityEstimator.Enabled:'
          'force_effective_connection_type/2G,'
          'PreviewsClientLoFi.Enabled:'
          'max_allowed_effective_connection_type/4G')

      test_driver.AddChromeArg(
          '--force-fieldtrials=NetworkQualityEstimator/Enabled/'
          'PreviewsClientLoFi/Enabled')

      # Fetch a non-SSL page with multiple images on it, such that the images
      # are fetched through the Data Reduction Proxy.
      test_driver.LoadURL('http://check.googlezip.net/static/index.html')

      image_response_count = 0
      for response in test_driver.GetHTTPResponses():
        if response.url.endswith('.png'):
          self.assertHasProxyHeaders(response)
          self.assertIn('range', response.request_headers)
          self.assertIn('content-range', response.response_headers)
          self.assertTrue(response.response_headers['content-range'].startswith(
              'bytes 0-2047/'))
          image_response_count = image_response_count + 1

      self.assertNotEqual(0, image_response_count)

      self.assertPreviewShownViaHistogram(test_driver, 'LoFi')

if __name__ == '__main__':
  IntegrationTest.RunAllTests()
