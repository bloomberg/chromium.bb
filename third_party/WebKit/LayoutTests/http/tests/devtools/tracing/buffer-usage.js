// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that buffer usage update are sent when recording trace events and TimelineLifecycleDelegate methods are properly invoked in the expected order.\n`);
  await TestRunner.loadModule('performance_test_runner');
  await TestRunner.showPanel('timeline');
  await TestRunner.loadHTML(`
      <p>
      Tests that buffer usage update are sent when recording trace events and
      TimelineLifecycleDelegate methods are properly invoked in the expected order.
      </p>
    `);

  TestTimelineControllerClient = function() {
    this._hadLoadingProgress = false;
  };

  TestTimelineControllerClient.prototype = {
    recordingProgress: function() {
      if (!controller)
        return;
      TestRunner.addResult('TimelineControllerClient.recordingProgress');
      controller.stopRecording();
      controller = null;
    },

    loadingStarted: function() {
      TestRunner.addResult('TimelineControllerClient.loadingStarted');
    },

    loadingProgress: function() {
      if (this._hadLoadingProgress)
        return;
      this._hadLoadingProgress = true;
      TestRunner.addResult('TimelineControllerClient.loadingProgress');
    },

    processingStarted: function() {
      TestRunner.addResult('TimelineControllerClient.processingStarted');
    },

    loadingComplete: function() {
      TestRunner.addResult('TimelineControllerClient.loadingComplete');
      TestRunner.completeTest();
    }
  };
  var performanceModel = new Timeline.PerformanceModel();
  var controller =
      new Timeline.TimelineController(TestRunner.tracingManager, performanceModel, new TestTimelineControllerClient());
  controller.startRecording({}, []).then(() => {
    TestRunner.addResult('TimelineControllerClient.recordingStarted');
  });
})();
