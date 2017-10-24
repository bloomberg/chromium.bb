// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests Comparison view of detailed heap snapshots. Shown node count must be preserved after sorting.\n`);
  await TestRunner.loadModule('heap_snapshot_test_runner');
  await TestRunner.showPanel('heap_profiler');
  await TestRunner.loadHTML(`
      <p>
      Tests Comparison view of detailed heap snapshots.
      Shown node count must be preserved after sorting.
      </p>
    `);

  var instanceCount = 24;
  function createHeapSnapshotA() {
    return HeapSnapshotTestRunner.createHeapSnapshot(instanceCount, 5);
  }
  function createHeapSnapshotB() {
    return HeapSnapshotTestRunner.createHeapSnapshot(instanceCount + 1, 5 + instanceCount);
  }

  HeapSnapshotTestRunner.runHeapSnapshotTestSuite([function testExpansionPreservedWhenSorting(next) {
    HeapSnapshotTestRunner.takeAndOpenSnapshot(createHeapSnapshotA, createSnapshotB);
    function createSnapshotB() {
      HeapSnapshotTestRunner.takeAndOpenSnapshot(createHeapSnapshotB, step1);
    }

    function step1() {
      HeapSnapshotTestRunner.switchToView('Comparison', step2);
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
        HeapSnapshotTestRunner.clickShowMoreButton('showAll', buttonsNode, step4);
      }
    }

    function step4() {
      var row = HeapSnapshotTestRunner.findRow('B');
      TestRunner.assertEquals(true, !!row, '"B" row');
      function deletedNodeMatcher(data) {
        return data._isDeletedNode && data._name.charAt(0) === 'B';
      }
      var bInstanceRow = HeapSnapshotTestRunner.findMatchingRow(deletedNodeMatcher, row);
      TestRunner.assertEquals(true, !!bInstanceRow, '"B" instance row');
      var buttonsNode = HeapSnapshotTestRunner.findButtonsNode(row, bInstanceRow);
      TestRunner.assertEquals(false, !!buttonsNode, 'buttons node found!');
      step5();
    }

    var nodeCount;
    function step5() {
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
