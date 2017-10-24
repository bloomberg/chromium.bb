// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests Containment view of detailed heap snapshots. The "Show All" button must show all nodes. Test object distances calculation.\n`);
  await TestRunner.loadModule('heap_snapshot_test_runner');
  await TestRunner.showPanel('heap_profiler');
  await TestRunner.loadHTML(`
      <p>
      Tests Containment view of detailed heap snapshots.
      The &quot;Show All&quot; button must show all nodes.
      Test object distances calculation.
      </p>
    `);

  var instanceCount = 25;
  function createHeapSnapshot() {
    return HeapSnapshotTestRunner.createHeapSnapshot(instanceCount);
  }

  HeapSnapshotTestRunner.runHeapSnapshotTestSuite([
    function testShowAll(next) {
      HeapSnapshotTestRunner.takeAndOpenSnapshot(createHeapSnapshot, step1);

      function step1() {
        HeapSnapshotTestRunner.switchToView('Containment', step2);
      }

      function step2() {
        HeapSnapshotTestRunner.findAndExpandWindow(step3);
      }

      function step3(row) {
        var buttonsNode = HeapSnapshotTestRunner.findButtonsNode(row);
        TestRunner.assertEquals(true, !!buttonsNode, 'buttons node');
        var words = buttonsNode.showAll.textContent.split(' ');
        for (var i = 0; i < words.length; ++i) {
          var maybeNumber = parseInt(words[i], 10);
          if (!isNaN(maybeNumber))
            TestRunner.assertEquals(
                instanceCount - row._dataGrid.defaultPopulateCount(), maybeNumber, buttonsNode.showAll.textContent);
        }
        HeapSnapshotTestRunner.clickShowMoreButton('showAll', buttonsNode, step4);
      }

      function step4(row) {
        var rowsShown = HeapSnapshotTestRunner.countDataRows(row);
        TestRunner.assertEquals(instanceCount, rowsShown, 'after showAll click');
        var buttonsNode = HeapSnapshotTestRunner.findButtonsNode(row);
        TestRunner.assertEquals(false, !!buttonsNode, 'buttons node found when all instances are shown!');
        setTimeout(next, 0);
      }
    },

    function testDistances(next) {
      function createHeapSnapshot() {
        // Mocking results of running the following code:
        //
        // function Tail() {}
        // next = new Tail();
        // for (var i = 0; i < 5; ++i)
        //   next = { next: next };

        var builder = new HeapSnapshotTestRunner.HeapSnapshotBuilder();
        var rootNode = builder.rootNode;

        var gcRootsNode =
            new HeapSnapshotTestRunner.HeapNode('(GC roots)', 0, HeapSnapshotTestRunner.HeapNode.Type.synthetic);
        rootNode.linkNode(gcRootsNode, HeapSnapshotTestRunner.HeapEdge.Type.element);

        var globalHandlesNode = new HeapSnapshotTestRunner.HeapNode('(Global Handles)');
        gcRootsNode.linkNode(globalHandlesNode, HeapSnapshotTestRunner.HeapEdge.Type.element);

        var nativeContextNode = new HeapSnapshotTestRunner.HeapNode('system / NativeContext', 32);
        globalHandlesNode.linkNode(nativeContextNode, HeapSnapshotTestRunner.HeapEdge.Type.element);

        var windowNode = new HeapSnapshotTestRunner.HeapNode('Window', 20);
        rootNode.linkNode(windowNode, HeapSnapshotTestRunner.HeapEdge.Type.shortcut);
        nativeContextNode.linkNode(windowNode, HeapSnapshotTestRunner.HeapEdge.Type.element);
        windowNode.linkNode(nativeContextNode, HeapSnapshotTestRunner.HeapEdge.Type.internal, 'native_context');

        var headNode = new HeapSnapshotTestRunner.HeapNode('Body', 32);
        windowNode.linkNode(headNode, HeapSnapshotTestRunner.HeapEdge.Type.property, 'next');
        for (var i = 1; i < 5; ++i) {
          var nextNode = new HeapSnapshotTestRunner.HeapNode('Body', 32);
          headNode.linkNode(nextNode, HeapSnapshotTestRunner.HeapEdge.Type.property, 'next');
          headNode = nextNode;
        }
        var tailNode = new HeapSnapshotTestRunner.HeapNode('Tail', 32);
        headNode.linkNode(tailNode, HeapSnapshotTestRunner.HeapEdge.Type.property, 'next');
        return builder.generateSnapshot();
      }

      var distance = 1;
      HeapSnapshotTestRunner.takeAndOpenSnapshot(createHeapSnapshot, step1);

      function step1() {
        HeapSnapshotTestRunner.switchToView('Containment', step2);
      }

      function step2() {
        HeapSnapshotTestRunner.findAndExpandGCRoots(step3);
      }

      function step3(row) {
        TestRunner.assertEquals('(GC roots)', row._name, '(GC roots) object');
        TestRunner.assertEquals('\u2212', row.data.distance, '(GC roots) distance should be zero');
        HeapSnapshotTestRunner.findAndExpandWindow(step4);
      }

      function step4(row) {
        TestRunner.assertEquals('Window', row._name, 'Window object');
        TestRunner.assertEquals(distance, row._distance, 'Window distance should be 1');
        var child = HeapSnapshotTestRunner.findMatchingRow(function(obj) {
          return obj._referenceName === 'next';
        }, row);
        TestRunner.assertEquals(true, !!child, 'next found');
        HeapSnapshotTestRunner.expandRow(child, step5);
      }

      function step5(row) {
        ++distance;
        TestRunner.assertEquals(distance, row._distance, 'Check distance of objects chain');
        if (row._name === 'Tail') {
          TestRunner.assertEquals(7, distance, 'Tail distance');
          setTimeout(next, 0);
          return;
        }
        TestRunner.assertEquals('Body', row._name, 'Body');
        var child = HeapSnapshotTestRunner.findMatchingRow(function(obj) {
          return obj._referenceName === 'next';
        }, row);
        TestRunner.assertEquals(true, !!child, 'next found');
        if (child._name !== 'Tail')
          HeapSnapshotTestRunner.expandRow(child, step5);
        else
          step5(child);
      }
    }
  ]);
})();
