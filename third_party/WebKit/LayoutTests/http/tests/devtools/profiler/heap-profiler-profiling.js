// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult('Tests that sampling heap profiling works.\n');
  await TestRunner.loadModule('heap_snapshot_test_runner');
  await TestRunner.showPanel('heap_profiler');

  HeapSnapshotTestRunner.runHeapSnapshotTestSuite([async function testProfiling(next) {

    HeapSnapshotTestRunner.startSamplingHeapProfiler();
    await TestRunner.evaluateInPagePromise(`
        function pageFunction() {
          (function () {
            window.holder = [];
            // Allocate few MBs of data.
            for (var i = 0; i < 1000; ++i)
              window.holder.push(new Array(1000).fill(42));
          })();
        }
        pageFunction();`);
    HeapSnapshotTestRunner.stopSamplingHeapProfiler();

    const view = await HeapSnapshotTestRunner.showProfileWhenAdded('Profile 1');
    const tree = view.profileDataGridTree;
    if (!tree)
      TestRunner.addResult('no tree');
    checkFunction(tree, 'pageFunction');
    checkFunction(tree, '(anonymous)');
    next();

    function checkFunction(tree, name) {
      let node = tree.children[0];
      if (!node)
        TestRunner.addResult('no node');
      while (node) {
        const url = node.element().children[2].lastChild.textContent;
        if (node.functionName === name) {
          TestRunner.addResult(`found ${name} ${url}`);
          return;
        }
        node = node.traverseNextNode(true, null, true);
      }
      TestRunner.addResult(`${name} is not found.`);
    }
  }]);

})();
