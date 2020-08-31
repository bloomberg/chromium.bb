# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re

from common import TestDriver
from common import IntegrationTest
from decorators import ChromeVersionEqualOrAfterM

LITEPAGES_REGEXP = r'https://\w+\.litepages\.googlezip\.net/.*'

class SubresourceRedirect(IntegrationTest):

  def enableSubresourceRedirectFeature(self, test_driver):
    test_driver.EnableChromeFeature('SubresourceRedirect<SubresourceRedirect')
    test_driver.AddChromeArg('--force-fieldtrials=SubresourceRedirect/Enabled')
    test_driver.AddChromeArg(
        '--force-fieldtrial-params='
        'SubresourceRedirect.Enabled:enable_subresource_server_redirect/true')
    test_driver.EnableChromeFeature('OptimizationHints')
    test_driver.EnableChromeFeature('OptimizationHintsFetching')
    test_driver.EnableChromeFeature(
        'OptimizationHintsFetchingAnonymousDataConsent')
    test_driver.AddChromeArg('--enable-spdy-proxy-auth')
    test_driver.AddChromeArg('--dont-require-litepage-redirect-infobar')

  # Verifies that image subresources on a page have been returned
  # from the compression server.
  @ChromeVersionEqualOrAfterM(77)
  def testCompressImage(self):
    with TestDriver() as test_driver:
      self.enableSubresourceRedirectFeature(test_driver)
      test_driver.LoadURL(
          'https://probe.googlezip.net/static/image_delayed_load.html')

      test_driver.SleepUntilHistogramHasEntry(
          'SubresourceRedirect.CompressionAttempt.ServerResponded')

      image_responses = 0
      for response in test_driver.GetHTTPResponses():
        content_type = ''
        if 'content-type' in response.response_headers:
          content_type = response.response_headers['content-type']
        if ('image/' in content_type
          and re.match(LITEPAGES_REGEXP, response.url)
          and 200 == response.status):
          image_responses += 1

      self.assertEqual(5, image_responses)

  # Verifies that when the image compression server serves a
  # redirect, then Chrome fetches the image directly.
  @ChromeVersionEqualOrAfterM(77)
  def testOnRedirectImage(self):
    with TestDriver() as test_driver:
      self.enableSubresourceRedirectFeature(test_driver)
      # Image compression server returns a 307 for all images on this webpage.
      test_driver.LoadURL(
        'https://testsafebrowsing.appspot.com/s/image_small.html')

      server_bypass = 0
      image_responses = 0

      for response in test_driver.GetHTTPResponses():
        content_type = ''
        if 'content-type' in response.response_headers:
          content_type = response.response_headers['content-type']
        if ('image/' in content_type
          and re.match(LITEPAGES_REGEXP, response.url)
          and 200 == response.status):
          image_responses += 1
        if ('https://testsafebrowsing.appspot.com/s/bad_assets/small.png'
          == response.url and 200 == response.status):
          server_bypass += 1

      self.assertEqual(1, server_bypass)
      self.assertEqual(0, image_responses)

  # Verifies that non-image subresources aren't redirected to the
  # compression server.
  @ChromeVersionEqualOrAfterM(77)
  def testNoCompressNonImage(self):
    with TestDriver() as test_driver:
      self.enableSubresourceRedirectFeature(test_driver)
      test_driver.LoadURL('https://probe.googlezip.net/testvideo.html')

      image_responses = 0

      for response in test_driver.GetHTTPResponses():
        content_type = ''
        if 'content-type' in response.response_headers:
          content_type = response.response_headers['content-type']
        if ('image/' in content_type
          and re.match(LITEPAGES_REGEXP, response.url)
          and 200 == response.status):
          image_responses += 1

      self.assertEqual(0, image_responses)

  # Verifies that non-secure connections aren't redirected to the
  # compression server.
  @ChromeVersionEqualOrAfterM(77)
  def testNoCompressNonHTTPS(self):
    with TestDriver() as test_driver:
      self.enableSubresourceRedirectFeature(test_driver)
      test_driver.LoadURL('http://probe.googlezip.net/static/index.html')

      image_responses = 0

      for response in test_driver.GetHTTPResponses():
        content_type = ''
        if 'content-type' in response.response_headers:
          content_type = response.response_headers['content-type']
        if ('image/' in content_type
          and re.match(LITEPAGES_REGEXP, response.url)
          and 200 == response.status):
          image_responses += 1

      self.assertEqual(0, image_responses)

if __name__ == '__main__':
  IntegrationTest.RunAllTests()
