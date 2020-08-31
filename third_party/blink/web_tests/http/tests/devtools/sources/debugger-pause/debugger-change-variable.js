// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that modifying local variables works fine.\n`);
  await TestRunner.loadModule('console_test_runner');
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function slave(x)
      {
          var y = 20;
          debugger;
      }

      function testFunction()
      {
          var localObject1 = { a: 310 };
          var localObject2 = 42;
          slave(4000);
      }
  `);

  SourcesTestRunner.startDebuggerTest(step1, true);

  function evalLocalVariables(callback) {
    ConsoleTestRunner.evaluateInConsoleAndDump('localObject1.a', next);
    function next() {
      ConsoleTestRunner.evaluateInConsoleAndDump('localObject2', callback);
    }
  }

  function localScopeObject() {
    var localsSection = SourcesTestRunner.scopeChainSections()[0];
    return localsSection._object;
  }

  function step1() {
    SourcesTestRunner.runTestFunctionAndWaitUntilPaused();
    TestRunner.addSniffer(
              Sources.CallStackSidebarPane.prototype, '_updatedForTest', step2);
  }

  function step2(callFrames) {
    var pane = self.runtime.sharedInstance(Sources.CallStackSidebarPane);
    pane._selectNextCallFrameOnStack();
    TestRunner.deprecatedRunAfterPendingDispatches(step3);
  }

  function step3() {
    TestRunner.addResult('\nEvaluated before modification:');
    evalLocalVariables(step4);
  }

  async function step4() {
    await localScopeObject().setPropertyValue('localObject1', '({ a: -290})');
    await localScopeObject().setPropertyValue({value: 'localObject2'}, '123');
    TestRunner.addResult('\nEvaluated after modification:');
    evalLocalVariables(step7);
  }

  function step7() {
    SourcesTestRunner.completeDebuggerTest();
  }
})();
