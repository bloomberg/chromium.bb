// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that debugger will skip breakpoint hit when script execution is already paused. See bug\n`);
  await TestRunner.loadModule('console_test_runner');
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.loadHTML(`
      <p>
      Tests that debugger will skip breakpoint hit when script execution is already paused. <a href="https://bugs.webkit.org/show_bug.cgi?id=41768">See bug</a>
      </p>
    `);
  await TestRunner.evaluateInPagePromise(`
      function testFunction()
      {
          testFunction.invocationCount++;
          debugger;
      }

      testFunction.invocationCount = 0;
  `);

  SourcesTestRunner.startDebuggerTest(step1);

  function step1() {
    SourcesTestRunner.runTestFunctionAndWaitUntilPaused(step2);
  }

  function step2() {
    ConsoleTestRunner.evaluateInConsole(
        'testFunction(); testFunction.invocationCount', step3);
    TestRunner.addResult('Set timer for test function.');
  }

  function step3(result) {
    TestRunner.addResult('testFunction.invocationCount = ' + result);
    SourcesTestRunner.completeDebuggerTest();
  }
})();
