// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Test recording page reload works in Timeline.\n`);
  await TestRunner.loadModule('performance_test_runner');
  await TestRunner.showPanel('timeline');
  TestRunner.resourceTreeModel.addEventListener(SDK.ResourceTreeModel.Events.Load, TestRunner.pageLoaded);

  var panel = UI.panels.timeline;
  PerformanceTestRunner.runWhenTimelineIsReady(recordingStopped);

  panel._millisecondsToRecordAfterLoadEvent = 1;
  panel._recordReload();

  function recordingStopped() {
    TestRunner.addResult('Recording stopped');
    TestRunner.completeTest();
  }
})();
