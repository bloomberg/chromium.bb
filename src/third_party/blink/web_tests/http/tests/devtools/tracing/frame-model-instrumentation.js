// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  await TestRunner.loadModule('performance_test_runner');
  await TestRunner.showPanel('timeline');
  await TestRunner.evaluateInPagePromise(`
  function doActions() {
    return generateFrames(3);
  }`);

  UI.panels.timeline._captureLayersAndPicturesSetting.set(true);
  await PerformanceTestRunner.invokeAsyncWithTimeline('doActions');
  const frames = PerformanceTestRunner.timelineFrameModel().frames();
  const lastFrame = PerformanceTestRunner.timelineFrameModel().frames().peekLast();
  if (lastFrame) {
    TestRunner.addResult('layerTree: ' + typeof lastFrame.layerTree);
    TestRunner.addResult('mainFrameId: ' + typeof lastFrame._mainFrameId);
    const paints = lastFrame.layerTree.paints();
    TestRunner.addResult('paints: ' + (paints && paints.length ? 'present' : 'absent'));
  } else {
    TestRunner.addResult('FAIL: there was no frame');
  }
  TestRunner.completeTest();
})();
