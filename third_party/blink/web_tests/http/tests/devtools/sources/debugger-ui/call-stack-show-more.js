// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests "Show more" button in CallStackSidebarPane.`);
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function callWithAsyncStack(f, depth) {
        if (depth === 0) {
          f();
          return;
        }
        wrapper = eval('(function call' + depth + '() { callWithAsyncStack(f, depth - 1) }) //# sourceURL=wrapper.js');
        Promise.resolve().then(wrapper);
      }
      function testFunction() {
        callWithAsyncStack(() => {debugger}, 36);
      }
      //# sourceURL=test.js
    `);

  await SourcesTestRunner.startDebuggerTestPromise(/* quiet */ true);
  await SourcesTestRunner.runTestFunctionAndWaitUntilPausedPromise();
  await TestRunner.addSnifferPromise(
      Sources.CallStackSidebarPane.prototype, '_updatedForTest');
  dumpCallStackSidebarPane();

  TestRunner.addResult('\n---------------\nClicks show more..');
  const pane = self.runtime.sharedInstance(Sources.CallStackSidebarPane);
  pane.contentElement.querySelector('.show-more-message > .link').click();
  await TestRunner.addSnifferPromise(
      Sources.CallStackSidebarPane.prototype, '_updatedForTest');
  dumpCallStackSidebarPane();
  SourcesTestRunner.completeDebuggerTest();

  function dumpCallStackSidebarPane() {
    const pane = self.runtime.sharedInstance(Sources.CallStackSidebarPane);
    for (const element of pane.contentElement.querySelectorAll(
             '.call-frame-item'))
      TestRunner.addResult(element.deepTextContent().replace(/VM\d+/g, 'VM'));
  }
})();
