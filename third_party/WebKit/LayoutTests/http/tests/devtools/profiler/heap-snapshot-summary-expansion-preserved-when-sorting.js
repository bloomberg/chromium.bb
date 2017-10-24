// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests Summary view of detailed heap snapshots. Expanded nodes must be preserved after sorting.\n`);
  await TestRunner.loadModule('heap_snapshot_test_runner');
  await TestRunner.showPanel('heap_profiler');
  await TestRunner.loadHTML(`
      <p>
      Tests Summary view of detailed heap snapshots.
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
      HeapSnapshotTestRunner.switchToView('Summary', step2);
    }

    function step2() {
      var row = HeapSnapshotTestRunner.findRow('B');
      TestRunner.assertEquals(true, !!row, '"B" row');
      HeapSnapshotTestRunner.expandRow(row, expandB);
      function expandB() {
        var buttonsNode = HeapSnapshotTestRunner.findButtonsNode(row);
        TestRunner.assertEquals(true, !!buttonsNode, 'no buttons node found!');
        HeapSnapshotTestRunner.clickShowMoreButton('showAll', buttonsNode, step3);
      }
    }

    var columns;
    function step3() {
      columns = HeapSnapshotTestRunner.viewColumns();
      HeapSnapshotTestRunner.clickColumn(columns[0], step4);
    }

    function step4() {
      var row = HeapSnapshotTestRunner.findRow('B');
      TestRunner.assertEquals(true, !!row, '"B" row');
      var bInstanceRow = row.children[0];
      TestRunner.assertEquals(true, !!bInstanceRow, '"B" instance row');
      HeapSnapshotTestRunner.expandRow(bInstanceRow, expandA);
      function expandA(row) {
        function propertyMatcher(data) {
          return data._referenceName === 'a' && data._name.charAt(0) === 'A';
        }
        var aRow = HeapSnapshotTestRunner.findMatchingRow(propertyMatcher, row);
        TestRunner.assertEquals(true, !!aRow, '"a: A" row');
        HeapSnapshotTestRunner.expandRow(aRow, step5);
      }
    }

    var columnContents;
    function step5() {
      columnContents = HeapSnapshotTestRunner.columnContents(columns[0]);
      HeapSnapshotTestRunner.clickColumn(columns[0], clickTwice);
      function clickTwice() {
        HeapSnapshotTestRunner.clickColumn(columns[0], step6);
      }
    }

    function step6() {
      var newColumnContents = HeapSnapshotTestRunner.columnContents(columns[0]);
      HeapSnapshotTestRunner.assertColumnContentsEqual(columnContents, newColumnContents);
      setTimeout(next, 0);
    }
  }]);
})();
