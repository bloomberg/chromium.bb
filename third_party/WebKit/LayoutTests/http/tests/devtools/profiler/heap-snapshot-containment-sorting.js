// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests sorting in Containment view of detailed heap snapshots.\n`);
  await TestRunner.loadModule('heap_snapshot_test_runner');
  await TestRunner.showPanel('heap_profiler');

  var instanceCount = 25;
  function createHeapSnapshot() {
    return HeapSnapshotTestRunner.createHeapSnapshot(instanceCount);
  }

  HeapSnapshotTestRunner.runHeapSnapshotTestSuite([function testSorting(next) {
    HeapSnapshotTestRunner.takeAndOpenSnapshot(createHeapSnapshot, step1);

    function step1() {
      HeapSnapshotTestRunner.switchToView('Containment', step2);
    }

    var gcRoots;
    var columns;
    var currentColumn;
    var currentColumnOrder;

    function step2() {
      HeapSnapshotTestRunner.findAndExpandGCRoots(step3);
    }

    function step3(gcRootsRow) {
      gcRoots = gcRootsRow;
      columns = HeapSnapshotTestRunner.viewColumns();
      currentColumn = 0;
      currentColumnOrder = false;
      setTimeout(step4, 0);
    }

    function step4() {
      if (currentColumn >= columns.length) {
        setTimeout(next, 0);
        return;
      }

      HeapSnapshotTestRunner.clickColumn(columns[currentColumn], step5);
    }

    function step5(newColumnState) {
      columns[currentColumn] = newColumnState;
      var contents = HeapSnapshotTestRunner.columnContents(columns[currentColumn], gcRoots);
      TestRunner.assertEquals(true, !!contents.length, 'column contents');
      var sortTypes = {object: 'name', distance: 'number', shallowSize: 'size', retainedSize: 'size'};
      TestRunner.assertEquals(true, !!sortTypes[columns[currentColumn].id], 'sort by id');
      HeapSnapshotTestRunner.checkArrayIsSorted(
          contents, sortTypes[columns[currentColumn].id], columns[currentColumn].sort);

      if (!currentColumnOrder)
        currentColumnOrder = true;
      else {
        currentColumnOrder = false;
        ++currentColumn;
      }
      setTimeout(step4, 0);
    }
  }]);
})();
