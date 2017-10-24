// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests Summary view of detailed heap snapshots. Shown node count must be preserved after sorting.\n`);
  await TestRunner.loadModule('heap_snapshot_test_runner');
  await TestRunner.showPanel('heap_profiler');
  await TestRunner.loadHTML(`
      <p>
      Tests Summary view of detailed heap snapshots.
      Shown node count must be preserved after sorting.
      </p>
    `);

  var instanceCount = 25;
  function createHeapSnapshot() {
    return HeapSnapshotTestRunner.createHeapSnapshot(instanceCount);
  }

  HeapSnapshotTestRunner.runHeapSnapshotTestSuite([function testExpansionPreservedWhenSorting(next) {
    HeapSnapshotTestRunner.takeAndOpenSnapshot(createHeapSnapshot, step1);

    function step1() {
      HeapSnapshotTestRunner.switchToView('Summary', step2);
    }

    var columns;
    function step2() {
      columns = HeapSnapshotTestRunner.viewColumns();
      HeapSnapshotTestRunner.clickColumn(columns[0], step3);
    }

    function step3() {
      var row = HeapSnapshotTestRunner.findRow('B');
      TestRunner.assertEquals(true, !!row, '"B" row');
      HeapSnapshotTestRunner.expandRow(row, showNext);
      function showNext(row) {
        var buttonsNode = HeapSnapshotTestRunner.findButtonsNode(row);
        TestRunner.assertEquals(true, !!buttonsNode, 'no buttons node found!');
        HeapSnapshotTestRunner.clickShowMoreButton('showNext', buttonsNode, step4);
      }
    }

    var nodeCount;
    function step4(row) {
      // There must be enough nodes to have some unrevealed.
      var buttonsNode = HeapSnapshotTestRunner.findButtonsNode(row);
      TestRunner.assertEquals(true, !!buttonsNode, 'no buttons node found!');

      nodeCount = HeapSnapshotTestRunner.columnContents(columns[0]).length;
      TestRunner.assertEquals(true, nodeCount > 0, 'nodeCount > 0');

      HeapSnapshotTestRunner.clickColumn(columns[0], clickTwice);
      function clickTwice() {
        HeapSnapshotTestRunner.clickColumn(columns[0], step6);
      }
    }

    function step6() {
      var newNodeCount = HeapSnapshotTestRunner.columnContents(columns[0]).length;
      TestRunner.assertEquals(nodeCount, newNodeCount);
      setTimeout(next, 0);
    }
  }]);
})();
