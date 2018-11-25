// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests async call stack for workers.\n`);
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(
      `
    var response = ` +
      '`' +
      `
    postMessage('ready');
    self.onmessage=function(e){
      debugger;
    }
    //# sourceURL=worker.js` +
      '`' +
      `;
    var blob = new Blob([response], {type: 'application/javascript'});
    function testFunction() {
      var worker = new Worker(URL.createObjectURL(blob));
      worker.onmessage = function(e) {
        worker.postMessage(42);
      };
    }`);

  SourcesTestRunner.startDebuggerTestPromise(/* quiet */ true)
      .then(() => SourcesTestRunner.runTestFunctionAndWaitUntilPausedPromise())
      .then(
          () => TestRunner.addSnifferPromise(
              Sources.CallStackSidebarPane.prototype, '_updatedForTest'))
      .then(() => dumpCallStackSidebarPane())
      .then(() => SourcesTestRunner.completeDebuggerTest());

  function dumpCallStackSidebarPane() {
    var pane = self.runtime.sharedInstance(Sources.CallStackSidebarPane);
    for (var element of pane.contentElement.querySelectorAll(
             '.call-frame-item'))
      TestRunner.addResult(element.deepTextContent()
                               .replace(/VM\d+/g, 'VM')
                               .replace(/blob:http:[^:]+/, 'blob'));
  }
})();
