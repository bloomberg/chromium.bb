// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that profile removal right after stop profiling issued works. Bug 476430.\n`);
  await TestRunner.loadModule('cpu_profiler_test_runner');
  await TestRunner.showPanel('js_profiler');

  CPUProfilerTestRunner.runProfilerTestSuite([async function testProfiling(next) {
    var cpuProfiler = TestRunner.cpuProfilerModel;
    var targetManager = SDK.targetManager;
    targetManager.addEventListener(SDK.TargetManager.Events.SuspendStateChanged, onSuspendStateChanged);
    var profilesPanel = UI.panels.js_profiler;
    TestRunner.addSniffer(cpuProfiler, 'stopRecording', stopRecording);
    TestRunner.addSniffer(profilesPanel, '_addProfileHeader', onAddProfileHeader);
    if (!UI.context.flavor(SDK.CPUProfilerModel))
      await new Promise(resolve => UI.context.addFlavorChangeListener(SDK.CPUProfilerModel, resolve));
    profilesPanel.toggleRecord();  // Start profiling.

    function onAddProfileHeader() {
      profilesPanel.toggleRecord();  // Stop profiling.
      profilesPanel._reset();        // ... and remove the profile before it actually stopped.
    }

    function onSuspendStateChanged() {
      if (SDK.targetManager.allTargetsSuspended()) {
        TestRunner.addResult('Suspending targets');
        return;
      }
      TestRunner.addResult('Resuming targets');
      CPUProfilerTestRunner.completeProfilerTest();
    }

    function stopRecording(resultPromise) {
      TestRunner.addResult('Stop recording command issued.');
      resultPromise.then(didStopRecording);
    }

    function didStopRecording(profile) {
      TestRunner.addResult('Recording stopped. profile (should be null): ' + profile);
    }
  }]);
})();
