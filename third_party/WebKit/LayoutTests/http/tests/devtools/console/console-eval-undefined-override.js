// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that evaluating something in console won't crash the browser if undefined value is overriden. The test passes if it doesn't crash. Bug 64155.\n`);

  await TestRunner.loadModule('console_test_runner');
  await TestRunner.showPanel('console');

  await TestRunner.loadHTML(`
    <p>
    Tests that evaluating something in console won't crash the browser if undefined value
    is overriden. The test passes if it doesn't crash.
    <a href="https://bugs.webkit.org/show_bug.cgi?id=64155">Bug 64155.</a>
    </p>
  `);

  ConsoleTestRunner.evaluateInConsole('var x = {a:1}; x.self = x; undefined = x;', step1);

  function step1() {
    ConsoleTestRunner.evaluateInConsole('unknownVar', step2);
  }

  function step2() {
    ConsoleTestRunner.dumpConsoleMessagesIgnoreErrorStackFrames();
    TestRunner.completeTest();
  }
})();
