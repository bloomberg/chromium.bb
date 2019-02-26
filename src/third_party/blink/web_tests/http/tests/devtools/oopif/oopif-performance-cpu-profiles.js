// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Test CPU profiles are recorded for OOPIFs.\n`);

  await TestRunner.loadModule('performance_test_runner');
  await TestRunner.showPanel('timeline');

  await PerformanceTestRunner.startTimeline();
  await TestRunner.navigatePromise('resources/page.html');
  await PerformanceTestRunner.stopTimeline();

  for (const track of PerformanceTestRunner.timelineModel().tracks().sort((a, b) => a.url.compareTo(b.url))) {
    if (track.type !== TimelineModel.TimelineModel.TrackType.MainThread)
      continue;
    TestRunner.addResult(`name: ${track.name}`);
    TestRunner.addResult(`url: ${track.url}`);
    TestRunner.addResult(`has CpuProfile: ${track.thread.events().some(e => e.name === 'CpuProfile')}\n`);
  }

  TestRunner.completeTest();
})();
