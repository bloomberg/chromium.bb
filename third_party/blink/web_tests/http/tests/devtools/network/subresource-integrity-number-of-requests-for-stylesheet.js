// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Verify that only one request is made for basic stylesheet requests with integrity attribute.\n`);
  await TestRunner.loadModule('network_test_runner');
  await TestRunner.loadModule('console_test_runner');
  await TestRunner.showPanel('network');

  await TestRunner.evaluateInPagePromise(`
      // Regression test for https://crbug.com/573269.
      function loadIFrame() {
          var iframe = document.createElement('iframe');
          iframe.src = 'resources/style-with-integrity-frame.html';
          document.body.appendChild(iframe);
      }
  `);

  ConsoleTestRunner.addConsoleSniffer(step1);
  TestRunner.evaluateInPage('loadIFrame()');

  async function step1() {
    var requests = NetworkTestRunner.findRequestsByURLPattern(/style.css/);
    TestRunner.assertTrue(requests.length === 1);
    await ConsoleTestRunner.dumpConsoleMessages();
    TestRunner.completeTest();
  }
})();
