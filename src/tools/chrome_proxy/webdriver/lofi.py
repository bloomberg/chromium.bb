# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import common
from common import TestDriver
from common import IntegrationTest
from decorators import ChromeVersionEqualOrAfterM

class LoFi(IntegrationTest):

  @ChromeVersionEqualOrAfterM(75)
  def testServerLoFiWithForcingFlag(self):
    with TestDriver() as test_driver:
      test_driver.AddChromeArg('--enable-spdy-proxy-auth')
      test_driver.EnableChromeFeature('Previews')
      test_driver.EnableChromeFeature('DataReductionProxyDecidesTransform')
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
      test_driver.EnableChromeFeature('Previews')
      test_driver.EnableChromeFeature('DataReductionProxyDecidesTransform')
      test_driver.AddChromeArg('--force-effective-connection-type=Slow-2G')

      test_driver.LoadURL('http://check.googlezip.net/static/index.html')

      responses = test_driver.GetHTTPResponses()
      self.assertNotEqual(len(responses), 0)
      for response in responses:
        if response.url.endswith('html'):
          self.assertNotIn('empty-image',
            response.response_headers['chrome-proxy'])

if __name__ == '__main__':
  IntegrationTest.RunAllTests()
