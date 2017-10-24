// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests Containment view of detailed heap snapshots. Expanded nodes must be preserved after sorting.\n`);
  await TestRunner.loadModule('heap_snapshot_test_runner');
  await TestRunner.showPanel('heap_profiler');
  await TestRunner.loadHTML(`
      <p>
      Tests Containment view of detailed heap snapshots.
      Expanded nodes must be preserved after sorting.
      </p>
    `);

  var instanceCount = 25;
  function createHeapSnapshot() {
    return HeapSnapshotTestRunner.createHeapSnapshot(instanceCount);
  }

  HeapSnapshotTestRunner.runHeapSnapshotTestSuite([function testExpansionPreservedWhenSorting(next) {
    HeapSnapshotTestRunner.takeAndOpenSnapshot(createHeapSnapshot, step1);

    function step1() {
      HeapSnapshotTestRunner.switchToView('Containment', step2);
    }

    var columns;
    function step2() {
      columns = HeapSnapshotTestRunner.viewColumns();
      HeapSnapshotTestRunner.clickColumn(columns[0], step3);
    }

    function step3() {
      HeapSnapshotTestRunner.findAndExpandWindow(step4);
    }

    function step4(row) {
      var buttonsNode = HeapSnapshotTestRunner.findButtonsNode(row);
      TestRunner.assertEquals(true, !!buttonsNode, 'no buttons node found!');
      HeapSnapshotTestRunner.clickShowMoreButton('showAll', buttonsNode, step5);
    }

    function step5(row) {
      var row = row.children[0];
      TestRunner.assertEquals(true, !!row, '"B" instance row');
      HeapSnapshotTestRunner.expandRow(row, expandA);
      function expandA(row) {
        function propertyMatcher(data) {
          return data._referenceName === 'a' && data._name.charAt(0) === 'A';
        }
        var aRow = HeapSnapshotTestRunner.findMatchingRow(propertyMatcher, row);
        TestRunner.assertEquals(true, !!aRow, '"a: A" row');
        HeapSnapshotTestRunner.expandRow(aRow, step6);
      }
    }

    var columnContents;
    function step6() {
      columnContents = HeapSnapshotTestRunner.columnContents(columns[0]);
      HeapSnapshotTestRunner.clickColumn(columns[0], clickTwice);
      function clickTwice() {
        HeapSnapshotTestRunner.clickColumn(columns[0], step7);
      }
    }

    function step7() {
      var newColumnContents = HeapSnapshotTestRunner.columnContents(columns[0]);
      HeapSnapshotTestRunner.assertColumnContentsEqual(columnContents, newColumnContents);
      setTimeout(next, 0);
    }
  }]);
})();
