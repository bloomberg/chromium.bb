// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult('Checks tool bar items for scripts');
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.showPanel('sources');

  await SourcesTestRunner.startDebuggerTestPromise();
  TestRunner.evaluateInPagePromise(`
    function testFunction() {
      eval('foo()//# sourceURL=test.js');

      function foo() {
        eval('debugger;');
      }
    }
    //# sourceURL=foo.js`);

  SourcesTestRunner.runTestFunctionAndWaitUntilPausedPromise();
  await TestRunner.addSnifferPromise(
            Sources.ScriptOriginPlugin.prototype, 'rightToolbarItems');

  TestRunner.addResult('Items for foo.js:');
  dumpToolbarItems(Sources.SourcesPanel.instance().visibleView);
  TestRunner.addResult('Items for test.js:');
  dumpToolbarItems(await SourcesTestRunner.showScriptSourcePromise('test.js'));

  SourcesTestRunner.completeDebuggerTest();

  function dumpToolbarItems(sourceFrame) {
    const items = sourceFrame.syncToolbarItems();
    for (let item of items)
      TestRunner.addResult(item.element.deepTextContent());
  }
})();
