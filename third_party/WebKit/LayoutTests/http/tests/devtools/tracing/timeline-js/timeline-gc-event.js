// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests the Timeline API instrumentation of a gc event\n`);
  await TestRunner.loadModule('performance_test_runner');
  await TestRunner.showPanel('timeline');
  await TestRunner.evaluateInPagePromise(`
      function produceGarbageForGCEvents()
      {
          if (window.testRunner) {
              window.gc();
              return new Promise((fulfill) => testRunner.layoutAndPaintAsyncThen(fulfill));
          }
          return Promise.reject();
      }
  `);

  PerformanceTestRunner.invokeAsyncWithTimeline('produceGarbageForGCEvents', validate);

  function validate() {
    var gcEvent = PerformanceTestRunner.findTimelineEvent(TimelineModel.TimelineModel.RecordType.MajorGC) ||
        PerformanceTestRunner.findTimelineEvent(TimelineModel.TimelineModel.RecordType.MinorGC);
    if (gcEvent)
      TestRunner.addResult('SUCCESS: Found expected GC event record');
    else
      TestRunner.addResult('FAIL: GC event record wasn\'t found');
    TestRunner.completeTest();
  }
})();
