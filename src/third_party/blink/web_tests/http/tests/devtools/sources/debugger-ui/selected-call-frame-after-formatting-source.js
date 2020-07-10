// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests selected call frame does not change when pretty-print is toggled.\n`);
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.loadModule('elements_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function testFunction()
      {
          return testFunction2();
      }

      function testFunction2()
      {
          var x = Math.sqrt(10);
          debugger;
          return x;
      }
  `);

  SourcesTestRunner.startDebuggerTest(step1);
  var panel = UI.panels.sources;
  var sourceFrame;

  function step1() {
    var testName = Root.Runtime.queryParam('test');
    testName = testName.substring(testName.lastIndexOf('/') + 1);
    SourcesTestRunner.showScriptSource(testName, step2);
  }

  function step2(frame) {
    sourceFrame = frame;
    SourcesTestRunner.runTestFunctionAndWaitUntilPaused(step3);
  }

  function step3() {
    SourcesTestRunner.completeDebuggerTest();
    return;
    TestRunner.debuggerModel.setSelectedCallFrame(TestRunner.debuggerModel.debuggerPausedDetails().callFrames[1]);
    sourceFrame._toggleFormatSource(step4);
  }

  function step4() {
    TestRunner.assertEquals('testFunction', UI.context.flavor(SDK.DebuggerModel.CallFrame).functionName);
    sourceFrame._toggleFormatSource(step5);
  }

  function step5() {
    SourcesTestRunner.completeDebuggerTest();
  }
})();
