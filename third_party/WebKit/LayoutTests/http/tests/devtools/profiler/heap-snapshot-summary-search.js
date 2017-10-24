// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests search in Summary view of detailed heap snapshots.\n`);
  await TestRunner.loadModule('heap_snapshot_test_runner');
  await TestRunner.showPanel('heap_profiler');

  var instanceCount = 25;
  function createHeapSnapshot() {
    return HeapSnapshotTestRunner.createHeapSnapshot(instanceCount);
  }

  HeapSnapshotTestRunner.runHeapSnapshotTestSuite([function testSearch(next) {
    HeapSnapshotTestRunner.takeAndOpenSnapshot(createHeapSnapshot, step1a);
    function addSearchResultSniffer(step) {
      function jumpToSearchResult() {
        step(HeapSnapshotTestRunner.currentProfileView()._searchResults.length);
      }
      TestRunner.addSniffer(HeapSnapshotTestRunner.currentProfileView(), '_jumpToSearchResult', jumpToSearchResult);
    }

    function addNodeSelectedSniffer(callback) {
      TestRunner.addSniffer(HeapSnapshotTestRunner.currentProfileView(), '_selectRevealedNode', callback);
    }

    function step1a() {
      HeapSnapshotTestRunner.switchToView('Summary', step1b);
    }

    function step1b() {
      var row = HeapSnapshotTestRunner.findRow('Window');
      TestRunner.assertEquals(true, !!row, '"Window" class row');
      HeapSnapshotTestRunner.expandRow(row, step1c);
    }

    function step1c(row) {
      TestRunner.assertEquals(1, row.children.length, 'single Window object');
      var windowRow = row.children[0];
      TestRunner.assertEquals(true, !!windowRow, '"Window" instance row');
      HeapSnapshotTestRunner.expandRow(windowRow, step2);
    }

    function step2() {
      addSearchResultSniffer(step3);
      HeapSnapshotTestRunner.currentProfileView().performSearch({query: 'window', caseSensitive: false});
    }

    function step3(resultCount) {
      TestRunner.assertEquals(1, resultCount, 'Search for existing node');
      addSearchResultSniffer(step4);
      HeapSnapshotTestRunner.currentProfileView().performSearch({query: 'foo', caseSensitive: false});
    }

    function step4(resultCount) {
      TestRunner.assertEquals(0, resultCount, 'Search for not-existing node');
      addNodeSelectedSniffer(step5);
      HeapSnapshotTestRunner.currentProfileView().performSearch({query: '@999', caseSensitive: false});
    }

    function step5(node) {
      TestRunner.assertEquals(false, !!node, 'Search for not-existing node by id');
      addNodeSelectedSniffer(step6);
      HeapSnapshotTestRunner.currentProfileView().performSearch({query: '@83', caseSensitive: false});
    }

    function step6(node) {
      TestRunner.assertEquals(true, !!node, 'Search for existing node by id');
      next();
    }
  }]);
})();
