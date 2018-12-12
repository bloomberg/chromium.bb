# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from common import ParseFlags
from common import TestDriver
from common import IntegrationTest
from decorators import AndroidOnly
from decorators import ChromeVersionEqualOrAfterM

from selenium.common.exceptions import TimeoutException

def enableLitePageServerPreviews(t):
  t.AddChromeArg('--enable-spdy-proxy-auth')
  t.AddChromeArg('--enable-features=Previews,'
                 'LitePageServerPreviews<DummyTrialGroup')
  t.AddChromeArg('--dont-require-litepage-redirect-infobar')
  t.AddChromeArg('--ignore-previews-blacklist')
  t.AddChromeArg('--force-effective-connection-type=2G')
  t.AddChromeArg('--force-fieldtrials=DummyTrialGroup/Enabled')
  t.AddChromeArg(
      '--force-fieldtrial-params=DummyTrialGroup.Enabled:'
      'lite_page_preview_experiment/external_chrome_integration_test')

# These are integration tests for server provided previews and the
# protocol that supports them.
class HttpsPreviews(IntegrationTest):
  def _AssertShowingLitePage(self, t, expectedText, expectedImages):
    """Asserts that Chrome has loaded a Lite Page from the litepages server.

    Args:
      t: the TestDriver object.
    """
    lite_page_responses = 0
    image_responses = 0

    for response in t.GetHTTPResponses():
      ct = response.response_headers['content-type']
      if 'text/html' in ct:
        self.assertRegexpMatches(response.url,
                                 r"https://\w+\.litepages\.googlezip\.net/")
        self.assertEqual(200, response.status)
        lite_page_responses += 1
      if 'image/' in ct:
        self.assertRegexpMatches(response.url,
                                 r"https://\w+\.litepages\.googlezip\.net/")
        self.assertEqual(200, response.status)
        image_responses += 1

    self.assertEqual(1, lite_page_responses)
    self.assertEqual(expectedImages, image_responses)

    bodyText = t.ExecuteJavascriptStatement('document.body.innerText')
    self.assertIn(expectedText, bodyText)

    # Sum these because the new UI is not enabled by default in M72.
    h1 = t.GetHistogram('Previews.OmniboxAction.LitePageRedirect', 5)
    h2 = t.GetHistogram('Previews.InfoBarAction.LitePageRedirect', 5)
    self.assertEqual(1, h1.get('count',0)+h2.get('count',0))

  def _AssertShowingOriginalPage(self, t, expectedURL, expectedStatus):
    """Asserts that Chrome has not loaded a Lite Page from the litepages server.

    Args:
      t: the TestDriver object.
    """
    html_responses = 0

    for response in t.GetHTTPResponses():
      if expectedURL == response.url:
        self.assertEqual(expectedStatus, response.status)
        html_responses += 1

    self.assertEqual(1, html_responses)

  # Verifies that a Lite Page is not served when the server returns a bypass.
  @ChromeVersionEqualOrAfterM(72)
  def testServerReturnsBypass(self):
    with TestDriver() as t:
      enableLitePageServerPreviews(t)
      url = 'https://mobilespeed-test.appspot.com/static/litepagetests/bypass.html'
      t.LoadURL(url)
      self._AssertShowingOriginalPage(t, url, 200)

  # Verifies that a Lite Page is not served when the server returns a 404.
  @ChromeVersionEqualOrAfterM(72)
  def testServerReturns404(self):
    with TestDriver() as t:
      enableLitePageServerPreviews(t)
      url = 'https://mobilespeed-test.appspot.com/404'
      t.LoadURL(url)
      self._AssertShowingOriginalPage(t, url, 404)

  # Verifies that a Lite Page is served when enabled and not blacklisted.
  @ChromeVersionEqualOrAfterM(72)
  def testServerReturnsLitePage(self):
    with TestDriver() as t:
      enableLitePageServerPreviews(t)
      t.LoadURL('https://mobilespeed-test.appspot.com/static/litepagetests/simple.html')
      self._AssertShowingLitePage(t, 'Hello world', 1)

  # Verifies that a Lite Page is served when the main frame response is a
  # redirect to a URL that is not blacklisted.
  @ChromeVersionEqualOrAfterM(72)
  def testServerReturnsLitePageAfterRedirect(self):
    with TestDriver() as t:
      enableLitePageServerPreviews(t)
      t.LoadURL('https://mobilespeed-test.appspot.com/redirect-to/static/litepagetests/simple.html')
      self._AssertShowingLitePage(t, 'Hello world', 1)

  # Verifies that a bad SSL interstitial is shown (instead of a Lite Page) when
  # the origin server has bad certificates.
  @ChromeVersionEqualOrAfterM(72)
  def testServerShowsBadSSLInterstitial(self):
    with TestDriver() as t:
      enableLitePageServerPreviews(t)
      url = 'https://expired.badssl.com/'
      t.LoadURL(url)
      self._AssertShowingOriginalPage(t, url, 200)
      # BadSSL onterstitials are not actually shown in webdriver tests (they
      # seem to be clicked through automatically). This histogram is incremented
      # after an interstitial has been clicked.
      histogram = t.GetHistogram('interstitial.ssl.visited_site_after_warning',
                                 1)
      self.assertEqual(1, histogram['count'])

  # Verifies that a safebrowsing interstitial is shown (instead of a Lite Page)
  # when the URL is marked as malware by safebrowsing.
  @AndroidOnly
  @ChromeVersionEqualOrAfterM(72)
  def testServerShowsSafebrowsingInterstitial(self):
    with TestDriver() as t:
      enableLitePageServerPreviews(t)
      try :
        # LoadURL will timeout when the interstital appears.
        t.LoadURL('https://testsafebrowsing.appspot.com/s/malware.html')
        self.fail('expected timeout')
      except TimeoutException:
        histogram = t.GetHistogram('SB2.ResourceTypes2.Unsafe')
        self.assertEqual(1, histogram['count'])

if __name__ == '__main__':
  IntegrationTest.RunAllTests()
