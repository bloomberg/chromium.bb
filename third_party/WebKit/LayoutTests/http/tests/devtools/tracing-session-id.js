// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that Tracing agent returns a session id upon a start that is matching one issued in trace events.\n`);
  await TestRunner.loadModule('performance_test_runner');
  await TestRunner.showPanel('timeline');
  await TestRunner.loadHTML(`
      <p style="transform: translateZ(10px)"> <!-- Force compositing so we have SetLayerTreeHostId event as well -->
      </p>
    `);

  PerformanceTestRunner.evaluateWithTimeline('(function() {})', processTracingEvents);

  function processTracingEvents() {
    PerformanceTestRunner.tracingModel().sortedProcesses().forEach(function(process) {
      process.sortedThreads().forEach(function(thread) {
        thread.events().forEach(processEvent);
      });
    });
    TestRunner.completeTest();
  }

  function processEvent(event) {
    var metadataEvents = [
      TimelineModel.TimelineModel.RecordType.SetLayerTreeId, TimelineModel.TimelineModel.RecordType.TracingStartedInPage
    ];

    if (!event.hasCategory(SDK.TracingModel.DevToolsMetadataEventCategory) || metadataEvents.indexOf(event.name) < 0)
      return;

    TestRunner.assertEquals(PerformanceTestRunner.timelineModel().sessionId(), event.args['data']['sessionId']);
    TestRunner.addResult('Got DevTools metadata event: ' + event.name);
  }
})();
