// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that buffer usage update are sent when recording trace events and TimelineLifecycleDelegate methods are properly invoked in the expected order.\n`);
  await TestRunner.loadModule('performance_test_runner');
  await TestRunner.showPanel('timeline');

  class TestTimelineControllerClient {
    constructor() {
      this._hadLoadingProgress = false;
    }

    recordingProgress() {
      if (!controller)
        return;
      TestRunner.addResult('TimelineControllerClient.recordingProgress');
      controller.stopRecording();
      controller = null;
    }

    loadingStarted() {
      TestRunner.addResult('TimelineControllerClient.loadingStarted');
    }

    loadingProgress() {
      if (this._hadLoadingProgress)
        return;
      this._hadLoadingProgress = true;
      TestRunner.addResult('TimelineControllerClient.loadingProgress');
    }

    processingStarted() {
      TestRunner.addResult('TimelineControllerClient.processingStarted');
    }

    loadingComplete() {
      TestRunner.addResult('TimelineControllerClient.loadingComplete');
      TestRunner.completeTest();
    }
  };

  let controller = new Timeline.TimelineController(SDK.targetManager.mainTarget(), new TestTimelineControllerClient());
  controller.startRecording({}, []).then(() => {
    TestRunner.addResult('TimelineControllerClient.recordingStarted');
  });
})();
