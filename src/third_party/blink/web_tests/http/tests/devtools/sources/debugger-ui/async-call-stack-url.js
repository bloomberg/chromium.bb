// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that call stack sidebar contains correct urls for call frames.\n`);
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.addScriptTag('../debugger/resources/unformatted-async.js');
  await TestRunner.evaluateInPagePromise(`
      function testFunction()
      {
          setTimeout(f2, 0);
      }
  `);

  await TestRunner.DebuggerAgent.setAsyncCallStackDepth(200);

  var scriptFormatter = await SourcesTestRunner.scriptFormatter();

  SourcesTestRunner.startDebuggerTest(step3);

  function step3() {
    SourcesTestRunner.showScriptSource('unformatted-async.js', step4);
  }

  function step4() {
    TestRunner.addSniffer(Sources.ScriptFormatterEditorAction.prototype, '_updateButton', step5);
    scriptFormatter._toggleFormatScriptSource();
  }

  function step5() {
    SourcesTestRunner.runTestFunctionAndWaitUntilPaused();
    TestRunner.addSniffer(Sources.CallStackSidebarPane.prototype, '_updatedForTest', step6);
  }

  function step6() {
    var pane = self.runtime.sharedInstance(Sources.CallStackSidebarPane);
    for (var element of pane.contentElement.querySelectorAll('.call-frame-item'))
      TestRunner.addResult(element.deepTextContent().replace(/VM\d+/g, 'VM'));
    SourcesTestRunner.completeDebuggerTest();
  }
})();
