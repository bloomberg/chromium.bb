// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that evaluating an expression with a syntax error in the console won't crash the browser. Bug 61194.\n`);

  await TestRunner.loadModule('console_test_runner');
  await TestRunner.showPanel('console');

  await TestRunner.loadHTML(`
    <p>
    Tests that evaluating an expression with a syntax error in the console won't crash the browser.
    <a href="https://bugs.webkit.org/show_bug.cgi?id=61194">Bug 61194.</a>
    </p>
  `);

  ConsoleTestRunner.evaluateInConsole('foo().', step1);

  function step1() {
    ConsoleTestRunner.dumpConsoleMessagesIgnoreErrorStackFrames();
    TestRunner.completeTest();
  }
})();
