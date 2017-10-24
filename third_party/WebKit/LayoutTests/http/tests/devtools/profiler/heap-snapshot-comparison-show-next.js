// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests Comparison view of detailed heap snapshots. Repeated clicks on "Show Next" button must show all nodes.\n`);
  await TestRunner.loadModule('heap_snapshot_test_runner');
  await TestRunner.showPanel('heap_profiler');
  await TestRunner.loadHTML(`
      <p>
      Tests Comparison view of detailed heap snapshots.
      Repeated clicks on &quot;Show Next&quot; button must show all nodes.
      </p>
    `);

  var instanceCount = 24;
  function createHeapSnapshotA() {
    return HeapSnapshotTestRunner.createHeapSnapshot(instanceCount, 5);
  }
  function createHeapSnapshotB() {
    return HeapSnapshotTestRunner.createHeapSnapshot(instanceCount + 1, 5 + instanceCount);
  }

  HeapSnapshotTestRunner.runHeapSnapshotTestSuite([function testShowNext(next) {
    HeapSnapshotTestRunner.takeAndOpenSnapshot(createHeapSnapshotA, createSnapshotB);
    function createSnapshotB() {
      HeapSnapshotTestRunner.takeAndOpenSnapshot(createHeapSnapshotB, step1);
    }

    function step1() {
      HeapSnapshotTestRunner.switchToView('Comparison', step2);
    }

    function step2() {
      var row = HeapSnapshotTestRunner.findRow('A');
      TestRunner.assertEquals(true, !!row, '"A" row');
      HeapSnapshotTestRunner.expandRow(row, step3);
    }

    function step3(row) {
      var expectedRowCountA = parseInt(row.data['addedCount']);
      var rowsShown = HeapSnapshotTestRunner.countDataRows(row, function(node) {
        return node.data.addedCount;
      });
      TestRunner.assertEquals(
          true, rowsShown <= expectedRowCountA,
          'shown more instances than created: ' + rowsShown + ' > ' + expectedRowCountA);
      if (rowsShown < expectedRowCountA) {
        var buttonsNode = HeapSnapshotTestRunner.findButtonsNode(row);
        TestRunner.assertEquals(true, !!buttonsNode, 'buttons node');
        HeapSnapshotTestRunner.clickShowMoreButton('showNext', buttonsNode, step3);
      } else if (rowsShown === expectedRowCountA)
        setTimeout(step4.bind(null, row), 0);
    }

    function step4(row) {
      var expectedRowCountB = parseInt(row.data['removedCount']);
      var rowsShown = HeapSnapshotTestRunner.countDataRows(row, function(node) {
        return node.data.removedCount;
      });
      TestRunner.assertEquals(
          true, rowsShown <= expectedRowCountB,
          'shown more instances than created: ' + rowsShown + ' > ' + expectedRowCountB);
      if (rowsShown < expectedRowCountB) {
        var buttonsNode = HeapSnapshotTestRunner.findButtonsNode(row);
        TestRunner.assertEquals(true, !!buttonsNode, 'buttons node');
        HeapSnapshotTestRunner.clickShowMoreButton('showNext', buttonsNode, step4);
      } else if (rowsShown === expectedRowCountB) {
        var buttonsNode = HeapSnapshotTestRunner.findButtonsNode(row);
        TestRunner.assertEquals(false, !!buttonsNode, 'buttons node found after all rows shown');
        setTimeout(next, 0);
      }
    }
  }]);
})();
