 # Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import common
from common import TestDriver
from common import IntegrationTest
from decorators import ChromeVersionEqualOrAfterM
from emulation_server import InvalidTLSHandler

class ProxyConnection(IntegrationTest):

  @ChromeVersionEqualOrAfterM(64)
  def testTLSInjectionAfterHandshake(self):
    port = common.GetOpenPort()
    with TestDriver() as t:
      t.AddChromeArg('--enable-spdy-proxy-auth')
      # The server should be 127.0.0.1, not localhost because the two are
      # treated differently in Chrome internals. Using localhost invalidates the
      # test.
      t.AddChromeArg(
        '--data-reduction-proxy-http-proxies=https://127.0.0.1:%d' % port)
      t.AddChromeArg(
        '--force-fieldtrials=DataReductionProxyConfigService/Disabled')
      t.UseEmulationServer(InvalidTLSHandler, port=port)

      t.LoadURL('http://check.googlezip.net/test.html')
      responses = t.GetHTTPResponses()
      # Expect responses with a bypass on a bad proxy. If the test failed, the
      # next assertion will fail because there will be no responses.
      self.assertEqual(2, len(responses))
      for response in responses:
        self.assertNotHasChromeProxyViaHeader(response)
      self.assertTrue(t.SleepUntilHistogramHasEntry('DataReductionProxy.'
        'InvalidResponseHeadersReceived.NetError'))

if __name__ == '__main__':
  IntegrationTest.RunAllTests()
