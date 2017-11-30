// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult('Tests that hyperlink auditing (ping) requests appear in network panel')
  await TestRunner.loadModule('network_test_runner');
  await TestRunner.showPanel('network');
  await TestRunner.loadHTML(`<a id="pingLink" href="#" ping="ping.js">ping</a>`);
  await TestRunner.evaluateInPagePromise(`
      if (window.testRunner)
          testRunner.overridePreference("WebKitHyperlinkAuditingEnabled", 1);
      function navigateLink()
      {
          var evt = document.createEvent("MouseEvents");
          evt.initMouseEvent("click");
          var link = document.getElementById("pingLink");
          link.dispatchEvent(evt);
      }
  `);

  TestRunner.addSniffer(SDK.NetworkDispatcher.prototype, 'requestWillBeSent', step2);
  TestRunner.evaluateInPage('navigateLink()');

  function step2() {
    // inspector-test.js appears in network panel occasionally in Safari on
    // Mac, so checking last request.
    var request = NetworkTestRunner.networkRequests().pop();

    TestRunner.addResult(request.url());
    TestRunner.addResult('resource.requestContentType: ' + request.requestContentType());

    TestRunner.completeTest();
  }
})();
