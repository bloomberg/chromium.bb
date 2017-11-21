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
      function continueInstall() {
        const scope = 'resources/v8-cache-iframe.html';
        let reg = registrations[scope];
        if (!reg)
          return Promise.reject(new Error('No registration'));
        if (!reg.installing)
          return Promise.reject(new Error('No installing service worker'));
        return new Promise(resolve => {
          var channel = new MessageChannel();
          channel.port1.onmessage = () => { resolve(); };
          reg.installing.postMessage({port: channel.port2}, [channel.port2]);
        });
      }
  `);

  const scriptURL = 'resources/v8-cache-worker.js';
  const scope = 'resources/v8-cache-iframe.html';
  const frameId = 'frame_id';

  await ApplicationTestRunner.registerServiceWorker(scriptURL, scope)
  // Need to suspend targets, because V8 doesn't produce the cache when the
  // debugger is loaded.
  await SDK.targetManager.suspendAllTargets();
  await new Promise(
        (r) =>
        PerformanceTestRunner.invokeAsyncWithTimeline('continueInstall', r));
  TestRunner.addResult('--- Trace events while installing -------------');
  PerformanceTestRunner.printTimelineRecordsWithDetails(
      TimelineModel.TimelineModel.RecordType.CompileScript);
  TestRunner.addResult('-----------------------------------------------');
  await ApplicationTestRunner.waitForActivated(scope);
  await TestRunner.addIframe(scope, {id: frameId});
  await new Promise(
        (r) =>
        PerformanceTestRunner.invokeAsyncWithTimeline('loadScript', r));
  TestRunner.addResult('--- Trace events while executing scripts ------');
  PerformanceTestRunner.printTimelineRecordsWithDetails(
      TimelineModel.TimelineModel.RecordType.CompileScript);
  TestRunner.addResult('-----------------------------------------------');
  TestRunner.completeTest();
})();
