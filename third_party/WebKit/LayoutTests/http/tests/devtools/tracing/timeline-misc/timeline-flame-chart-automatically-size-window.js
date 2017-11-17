// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests the TimelineFlameChart automatically sized window.\n`);
  await TestRunner.loadModule('console_test_runner');
  await TestRunner.loadModule('performance_test_runner');
  await TestRunner.showPanel('timeline');
  await TestRunner.addScriptTag('../resources/timeline-data.js');

  var timeline = UI.panels.timeline;
  timeline._onModeChanged();
  timeline._currentView._automaticallySizeWindow = true;

  function requestWindowTimesHook(startTime, endTime) {
    if (startTime)
      TestRunner.addResult('time delta: ' + (endTime - startTime));
  }

  timeline.requestWindowTimes = requestWindowTimesHook;
  await PerformanceTestRunner.loadTimeline(PerformanceTestRunner.timelineData());
  TestRunner.completeTest();
})();
