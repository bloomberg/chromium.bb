// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests console message when script is loaded with incorrect text/html mime type and the URL contains the '^' character.\n`);
  await TestRunner.loadModule('console_test_runner');
  await TestRunner.showPanel('network');
  await TestRunner.loadHTML(`
      <p>Tests console message when script is loaded with incorrect text/html mime
      type and the URL contains the '^' character.</p>
      <a href="https://bugs.webkit.org/show_bug.cgi?id=103248">Bug 103248</a>
    `);
  await TestRunner.evaluateInPagePromise(`
      function loadScript()
      {
          var s = document.createElement("script");
          s.src = "resources/this-is-a-weird?querystring=with^carats^like^these^because^who^doesnt^love^strange^characters^in^urls";
          document.body.appendChild(s);
      }
  `);

  ConsoleTestRunner.addConsoleSniffer(step1);
  TestRunner.evaluateInPage('loadScript()');

  function step1() {
    ConsoleTestRunner.dumpConsoleMessages();
    TestRunner.completeTest();
  }
})();
