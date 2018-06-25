// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Test that function objects have source links in heap snapshot view.\n`);

  await TestRunner.loadModule('heap_profiler_test_runner');
  await TestRunner.showPanel('heap_profiler');
  await TestRunner.evaluateInPagePromise(`
      class MyTestClass {
        constructor() {
          this.z = new Uint32Array(10000);  // Pull the class to top.
          this.myFunction = () => 42;
        }
      };
      window.myTestClass = new MyTestClass();
      //# sourceURL=my-test-script.js`);

  await HeapProfilerTestRunner.takeSnapshotPromise();
  await HeapProfilerTestRunner.switchToView('Summary');

  const classRow = await HeapProfilerTestRunner.findAndExpandRow('MyTestClass');
  const instanceRow = classRow.children[0];
  await HeapProfilerTestRunner.expandRowPromise(instanceRow);
  const myFunctionRow = HeapProfilerTestRunner.findMatchingRow(n => n._referenceName === 'myFunction', instanceRow);

  let linkNode;
  do {
    linkNode = myFunctionRow._element.querySelector('.heap-object-source-link .devtools-link');
    await new Promise(r => requestAnimationFrame(r));
  } while (!linkNode);

  TestRunner.addResult(`Function source: ${linkNode.textContent}`);
  TestRunner.completeTest();
})();
