// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that evaluation in console still works even if script evals are prohibited by Content-Security-Policy. Bug 60800.\n`);

  await TestRunner.loadModule('console_test_runner');
  await TestRunner.showPanel('console');

  await TestRunner.loadHTML(`
    <p>
    Tests that evaluation in console still works even if script evals are prohibited by Content-Security-Policy.
    <a href="https://bugs.webkit.org/show_bug.cgi?id=60800">Bug 60800.</a>
    </p>
  `);

  ConsoleTestRunner.evaluateInConsole('1+2', step1);

  function step1() {
    ConsoleTestRunner.dumpConsoleMessages();
    TestRunner.completeTest();
  }
})();
