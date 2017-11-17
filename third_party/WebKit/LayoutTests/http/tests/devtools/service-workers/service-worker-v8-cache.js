// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests V8 cache information of Service Worker Cache Storage in timeline\n`);
  await TestRunner.loadModule('performance_test_runner');
  await TestRunner.loadModule('application_test_runner');
  await TestRunner.showPanel('resources');
  await TestRunner.showPanel('timeline');
  await TestRunner.evaluateInPagePromise(`
      function loadScript() {
        const url = 'v8-cache-script.js';
        const frameId = 'frame_id';
        let iframeWindow = document.getElementById(frameId).contentWindow;
        return iframeWindow.loadScript(url)
          .then(() => iframeWindow.loadScript(url));
      }
  `);

  const scriptURL = 'resources/v8-cache-worker.js';
  const scope = 'resources/v8-cache-iframe.html';
  const frameId = 'frame_id';

  ApplicationTestRunner.registerServiceWorker(scriptURL, scope)
      .then(_ => ApplicationTestRunner.waitForActivated(scope))
      .then(() => {
        return TestRunner.addIframe(scope, {id: frameId});
      })
      .then(() => {
        // Need to suspend targets, because V8 doesn't produce the cache when
        // the debugger is loaded.
        return SDK.targetManager.suspendAllTargets();
      })
      .then(() => {
        return new Promise((r) => {
          PerformanceTestRunner.invokeAsyncWithTimeline('loadScript', r);
        });
      })
      .then(() => {
        PerformanceTestRunner.printTimelineRecordsWithDetails(TimelineModel.TimelineModel.RecordType.CompileScript);
        TestRunner.completeTest();
      });
})();
