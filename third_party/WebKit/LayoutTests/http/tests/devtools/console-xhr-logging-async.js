// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that XMLHttpRequest Logging works when Enabled and doesn't show logs when Disabled for asynchronous XHRs.\n`);
  await TestRunner.loadModule('console_test_runner');
  await TestRunner.loadModule('network_test_runner');
  await TestRunner.loadHTML(`
      <a href="https://bugs.webkit.org/show_bug.cgi?id=79229">Bug 79229</a>
    `);

  step1();

  function makeRequest(callback) {
    NetworkTestRunner.makeSimpleXHR('GET', 'resources/xhr-exists.html', true, callback);
  }

  function step1() {
    Common.settingForTest('monitoringXHREnabled').set(true);
    makeRequest(step2);
  }

  function step2() {
    Common.settingForTest('monitoringXHREnabled').set(false);
    makeRequest(step3);
  }

  function step3() {
    function finish() {
      ConsoleTestRunner.dumpConsoleMessages();
      TestRunner.completeTest();
    }
    TestRunner.deprecatedRunAfterPendingDispatches(finish);
  }
})();
