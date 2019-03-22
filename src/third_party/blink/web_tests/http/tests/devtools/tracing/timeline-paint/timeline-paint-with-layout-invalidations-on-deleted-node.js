// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests the Timeline API instrumentation of layout invalidations on a deleted node.\n`);
  await TestRunner.loadModule('performance_test_runner');
  await TestRunner.showPanel('timeline');
  await TestRunner.loadHTML(`
      <!DOCTYPE HTML>
      <div id="testElement">FAIL - this should not be present when the test finishes.</div>
      <iframe src="../resources/timeline-iframe-paint.html" style="position: absolute; left: 40px; top: 40px; width: 100px; height: 100px; border: none"></iframe>
    `);
  await TestRunner.evaluateInPagePromise(`
      function display()
      {
          document.body.style.backgroundColor = "blue";
          var element = document.getElementById("testElement");
          element.style.width = "100px";
          var forceLayout = document.body.offsetTop;
          element.parentElement.removeChild(element);
          return waitForFrame();
      }

      function updateSubframeAndDisplay()
      {
          var element = frames[0].document.body.children[0];
          element.style.width = "200px";
          var forceLayout = frames[0].document.body.offsetTop;
          element.parentElement.removeChild(element);
          return waitForFrame();
      }
  `);

  Runtime.experiments.enableForTest('timelineInvalidationTracking');

  TestRunner.runTestSuite([
    async function testLocalFrame(next) {
      await PerformanceTestRunner.invokeAsyncWithTimeline('display');
      PerformanceTestRunner.dumpInvalidations(TimelineModel.TimelineModel.RecordType.Paint, 0, 'paint invalidations');
      next();
    },

    async function testSubframe(next) {
      await PerformanceTestRunner.invokeAsyncWithTimeline('updateSubframeAndDisplay');
      // The first paint corresponds to the local frame and should have no invalidations.
      var firstPaintEvent = PerformanceTestRunner.findTimelineEvent(TimelineModel.TimelineModel.RecordType.Paint);
      var firstInvalidations = TimelineModel.InvalidationTracker.invalidationEventsFor(firstPaintEvent);
      TestRunner.assertEquals(firstInvalidations, null);

      // We don't invalidate paint of deleted elements which doesn't affect other elements.
      var secondPaintEvent = PerformanceTestRunner.findTimelineEvent(TimelineModel.TimelineModel.RecordType.Paint);
      var secondInvalidations = TimelineModel.InvalidationTracker.invalidationEventsFor(secondPaintEvent);
      TestRunner.assertEquals(secondInvalidations, null);

      next();
    }
  ]);
})();
