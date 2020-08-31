// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that breakpoints are successfully restored after debugger disabling.\n`);
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function testFunction()
      {
          return 0;
      }
  `);

  SourcesTestRunner.startDebuggerTest(step1);

  function step1() {
    SourcesTestRunner.showScriptSource('debugger-disable-enable.js', step2);
  }

  async function step2(sourceFrame) {
    TestRunner.addResult('Main resource was shown.');
    await SourcesTestRunner.setBreakpoint(sourceFrame, 11, '', true);
    TestRunner.debuggerModel.addEventListener(SDK.DebuggerModel.Events.DebuggerWasDisabled, step3, this);
    TestRunner.debuggerModel._disableDebugger();
  }

  function step3() {
    TestRunner.addResult('Debugger disabled.');
    TestRunner.addResult('Evaluating test function.');
    TestRunner.evaluateInPage('testFunction()', step4);
  }

  function step4() {
    TestRunner.addResult('function evaluated without a pause on the breakpoint.');
    TestRunner.debuggerModel.addEventListener(SDK.DebuggerModel.Events.DebuggerWasEnabled, step5, this);
    TestRunner.debuggerModel._enableDebugger();
  }

  function step5() {
    TestRunner.addResult('Debugger was enabled');
    SourcesTestRunner.runTestFunctionAndWaitUntilPaused(step6);
  }

  function step6() {
    SourcesTestRunner.completeDebuggerTest();
  }
})();
