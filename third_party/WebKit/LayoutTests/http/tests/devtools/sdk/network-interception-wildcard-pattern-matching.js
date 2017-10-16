// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult("Test to ensure the consistency of front-end patterns vs backend patterns for request interception.\n");

  // Backend supports wildcards, but front-end does not. This test is to ensure the basic stability with wildcard characters.
  var urlPrefix = SDK.targetManager.mainTarget().inspectedURL().replace(/\/[^\/]*$/, '');
  var resourceURL = urlPrefix + '/bar.js';
  await checkPattern('**bar.js');
  await checkPattern(resourceURL);
  await checkPattern('*bar.js');
  await checkPattern('bar.js');
  await checkPattern('\*bar.js');
  await checkPattern('*b*');
  await checkPattern('*');
  await checkPattern('*bar_js');
  await checkPattern('*bar?js');

  TestRunner.completeTest();

  /**
   * @param {string} pattern
   * @return {!Promise}
   */
  async function checkPattern(pattern) {
    await SDK.multitargetNetworkManager.setInterceptionHandlerForPatterns([pattern], interceptionHandler);
    TestRunner.addResult("Requesting: bar.js");
    await TestRunner.evaluateInPageAsync(`fetch('` + resourceURL + `')`);
    TestRunner.addResult("Received: bar.js");
    await SDK.multitargetNetworkManager.setInterceptionHandlerForPatterns([], interceptionHandler);
    TestRunner.addResult("");

    /**
     * @param {!SDK.MultitargetNetworkManager.InterceptedRequest} interceptedRequest
     * @return {!Promise}
     */
    function interceptionHandler(interceptedRequest) {
      TestRunner.addResult("Received File: " + cleanURL(interceptedRequest.request.url));
      TestRunner.addResult("Pattern: " + cleanURL(pattern));
      return Promise.resolve();
    }
  }

  /**
   * @param {string} url
   * @return {string}
   */
  function cleanURL(url) {
    console.assert(url.startsWith(urlPrefix));
    return '(MASKED_URL_PATH)' + url.substr(urlPrefix.length);
  }
})();
