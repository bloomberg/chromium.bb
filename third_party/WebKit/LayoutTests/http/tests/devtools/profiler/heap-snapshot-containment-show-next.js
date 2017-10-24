// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests Containment view of detailed heap snapshots. Repeated clicks on "Show Next" button must show all nodes.\n`);
  await TestRunner.loadModule('heap_snapshot_test_runner');
  await TestRunner.showPanel('heap_profiler');
  await TestRunner.loadHTML(`
      <p>
      Tests Containment view of detailed heap snapshots.
      Repeated clicks on &quot;Show Next&quot; button must show all nodes.
      </p>
    `);

  var instanceCount = 25;
  function createHeapSnapshot() {
    return HeapSnapshotTestRunner.createHeapSnapshot(instanceCount);
  }

  HeapSnapshotTestRunner.runHeapSnapshotTestSuite([function testShowNext(next) {
    HeapSnapshotTestRunner.takeAndOpenSnapshot(createHeapSnapshot, step1);

    function step1() {
      HeapSnapshotTestRunner.switchToView('Containment', step2);
    }

    function step2() {
      HeapSnapshotTestRunner.findAndExpandWindow(step3);
    }

    function step3(row) {
      var rowsShown = HeapSnapshotTestRunner.countDataRows(row);
      TestRunner.assertEquals(
          true, rowsShown <= instanceCount, 'shown more instances than created: ' + rowsShown + ' > ' + instanceCount);
      if (rowsShown < instanceCount) {
        var buttonsNode = HeapSnapshotTestRunner.findButtonsNode(row);
        TestRunner.assertEquals(true, !!buttonsNode, 'buttons node');
        HeapSnapshotTestRunner.clickShowMoreButton('showNext', buttonsNode, step3);
      } else if (rowsShown === instanceCount) {
        var buttonsNode = HeapSnapshotTestRunner.findButtonsNode(row);
        TestRunner.assertEquals(false, !!buttonsNode, 'buttons node found when all instances are shown!');
        setTimeout(next, 0);
      }
    }
  }]);
})();
