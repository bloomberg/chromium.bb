# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import re
import time
import urllib
from common import ParseFlags
from common import TestDriver
from common import IntegrationTest
from decorators import AndroidOnly
from decorators import ChromeVersionEqualOrAfterM

from selenium.common.exceptions import TimeoutException

NAV_THROTTLE_VERSION = "v1_NavThrottle"
URL_LOADER_VERSION = "v2_URLLoader"

# These are integration tests for server provided previews and the
# protocol that supports them. This class is intended as an abstract base class
# to allow the two versions of this feature implementation to reuse the same
# tests.
class HttpsPreviewsBaseClass():
  # Abstract function for subclasses to override.
  def getVersion(self):
    raise Exception( "Please override this method")

  def EnableLitePageServerPreviewsAndInit(self, t):
    version = self.getVersion()

    # These feature flags are common to both versions.
    features = [
      "Previews",
      "LitePageServerPreviews",
      # Just in case NetworkService is on. Has no effect otherwise.
      "DataReductionProxyEnabledWithNetworkService",
    ]

    if version == NAV_THROTTLE_VERSION:
      # No additional flags here, but explicitly check it given the else below.
      pass
    elif version == URL_LOADER_VERSION:
      features += [
        "HTTPSServerPreviewsUsingURLLoader",
        "NetworkService",
      ]
    else:
      raise Exception('"%s" is not a valid version' % version)

    t.AddChromeArg('--enable-features=' + ','.join(features))

    t.AddChromeArg('--enable-spdy-proxy-auth')
    t.AddChromeArg('--dont-require-litepage-redirect-infobar')
    t.AddChromeArg('--ignore-previews-blocklist')
    t.AddChromeArg('--force-effective-connection-type=2G')
    t.AddChromeArg('--ignore-litepage-redirect-optimization-blacklist')
    t.AddChromeArg('--data-reduction-proxy-experiment='
      'external_chrome_integration_test')

    # Start Chrome and wait for initialization.
    t.LoadURL('data:,')
    t.SleepUntilHistogramHasEntry(
        'DataReductionProxy.ConfigService.FetchResponseCode')

  def _AssertShowingLitePage(self, t, expectedText, expectedImages):
    """Asserts that Chrome has loaded a Lite Page from the litepages server.

    Args:
      t: the TestDriver object.
    """
    lite_page_responses = 0
    image_responses = 0

    for response in t.GetHTTPResponses():
      content_type = ''
      if 'content-type' in response.response_headers:
        content_type = response.response_headers['content-type']

      if 'text/html' in content_type:
        self.assertRegexpMatches(response.url,
                                 r"https://\w+\.litepages\.googlezip\.net/")
        self.assertEqual(200, response.status)
        lite_page_responses += 1
      if 'image/' in content_type:
        self.assertRegexpMatches(response.url,
                                 r"https://\w+\.litepages\.googlezip\.net/")
        self.assertEqual(200, response.status)
        image_responses += 1

    self.assertEqual(1, lite_page_responses)
    self.assertEqual(expectedImages, image_responses)

    bodyText = t.ExecuteJavascriptStatement('document.body.innerText')
    self.assertIn(expectedText, bodyText)

    self.assertPreviewShownViaHistogram(t, 'LitePageRedirect')

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

    self.assertPreviewNotShownViaHistogram(t, 'LitePageRedirect')

  # Verifies that a Lite Page is not served when the server returns a bypass.
  @ChromeVersionEqualOrAfterM(74)
  def testServerReturnsBypass(self):
    with TestDriver() as t:
      self.EnableLitePageServerPreviewsAndInit(t)
      url = 'https://mobilespeed-test.appspot.com/static/litepagetests/bypass.html'
      t.LoadURL(url)
      self._AssertShowingOriginalPage(t, url, 200)

  # Verifies that a Lite Page is not served when the server returns a 404.
  @ChromeVersionEqualOrAfterM(74)
  def testServerReturns404(self):
    with TestDriver() as t:
      self.EnableLitePageServerPreviewsAndInit(t)
      url = 'https://mobilespeed-test.appspot.com/404'
      t.LoadURL(url)
      self._AssertShowingOriginalPage(t, url, 404)

  # Verifies that a Lite Page is served when enabled and not blacklisted.
  @ChromeVersionEqualOrAfterM(74)
  def testServerReturnsLitePage(self):
    with TestDriver() as t:
      self.EnableLitePageServerPreviewsAndInit(t)
      t.LoadURL('https://mobilespeed-test.appspot.com/static/litepagetests/simple.html')
      self._AssertShowingLitePage(t, 'Hello world', 1)

  # Verifies that a Lite Page pageload sends a DRP pingback.
  # TODO(robertogden): Set this to M73 once merged.
  @ChromeVersionEqualOrAfterM(74)
  def testPingbackSent(self):
    with TestDriver() as t:
      t.AddChromeArg('--enable-data-reduction-proxy-force-pingback')
      self.EnableLitePageServerPreviewsAndInit(t)

      # Navigate twice so that the first page sends a pingback. The second page
      # can be anything since only the first pageload will send a pingback in
      # this test.
      t.LoadURL('https://mobilespeed-test.appspot.com/static/litepagetests/simple.html')
      self._AssertShowingLitePage(t, 'Hello world', 1)
      t.LoadURL('https://www.google.com')

      t.SleepUntilHistogramHasEntry("DataReductionProxy.Pingback.Succeeded")
      # Verify one pingback attempt that was successful.
      attempted = t.GetHistogram('DataReductionProxy.Pingback.Attempted')
      self.assertEqual(1, attempted['count'])
      succeeded = t.GetHistogram('DataReductionProxy.Pingback.Succeeded')
      self.assertEqual(1, succeeded['count'])

  # Verifies that a Lite Page is served when the main frame response is a
  # redirect to a URL that is not blacklisted.
  @ChromeVersionEqualOrAfterM(74)
  def testServerReturnsLitePageAfterRedirect(self):
    with TestDriver() as t:
      self.EnableLitePageServerPreviewsAndInit(t)
      t.LoadURL('https://mobilespeed-test.appspot.com/redirect-to/static/litepagetests/simple.html')
      self._AssertShowingLitePage(t, 'Hello world', 1)

  # Verifies that a bad SSL interstitial is shown (instead of a Lite Page) when
  # the origin server has bad certificates.
  @ChromeVersionEqualOrAfterM(74)
  def testServerShowsBadSSLInterstitial(self):
    with TestDriver() as t:
      self.EnableLitePageServerPreviewsAndInit(t)
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
  @ChromeVersionEqualOrAfterM(74)
  def testServerShowsSafebrowsingInterstitial(self):
    with TestDriver() as t:
      self.EnableLitePageServerPreviewsAndInit(t)
      try :
        # LoadURL will timeout when the interstital appears.
        t.LoadURL('https://testsafebrowsing.appspot.com/s/malware.html')
        self.fail('expected timeout')
      except TimeoutException:
        histogram = t.GetHistogram('SB2.ResourceTypes2.Unsafe')
        self.assertEqual(1, histogram['count'])


  # Verifies that a Lite Page is served, an intervention report has been
  # sent to the correct reporting endpoint, and the content of this report
  # is expected.
  @ChromeVersionEqualOrAfterM(74)
  def testLitePageWebReport(self):
    with TestDriver() as t:
      t.AddChromeArg('--short-reporting-delay')
      t.UseNetLog()
      self.EnableLitePageServerPreviewsAndInit(t)

      t.LoadURL('https://mobilespeed-test.appspot.com/snapshot-test/')

      # Verify that the request is served by a Lite Page.
      lite_page_responses = 0
      lite_page_regexp = re.compile('https://\w+\.litepages\.googlezip\.net/p')
      for response in t.GetHTTPResponses():
        if lite_page_regexp.search(response.url) and response.status == 200:
          lite_page_responses += 1
      self.assertEqual(1, lite_page_responses)

      # Wait for intervention report to be attempted.
      t.SleepUntilHistogramHasEntry("Net.Reporting.ReportDeliveredAttempts",
        120)
      events = t.StopAndGetNetLog()["events"]

      # Collect IDs of expected reporting requests.
      report_request_id = []
      for event in events:
        if not "params" in event or not "headers" in event["params"]:
          continue

        header = event["params"]["headers"]
        quoted_report_url = urllib.quote_plus(
            "https://mobilespeed-test.appspot.com/web-reports")
        if ((":path: /webreports?u=%s" % quoted_report_url) in header
            and "content-type: application/reports+json" in header):
          report_request_id.append(event["source"]["id"])
      self.assertNotEqual(0, len(report_request_id))

      # Verify that at least one reporting request got 200.
      ok_responses = 0
      for id in report_request_id:
        for event in events:
          if (event["source"]["id"] != id
              or not "params" in event
              or not "headers" in event["params"]):
            continue

          for value in event["params"]["headers"]:
            if ":status: 200" in value:
              ok_responses += 1
      self.assertNotEqual(0, ok_responses)

class HttpsPreviewsNavigationThrottle(HttpsPreviewsBaseClass, IntegrationTest):
  def getVersion(self):
    return NAV_THROTTLE_VERSION

class HttpsPreviewsURLLoader(HttpsPreviewsBaseClass, IntegrationTest):
  def getVersion(self):
    return URL_LOADER_VERSION

if __name__ == '__main__':
  IntegrationTest.RunAllTests()
