// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests V8 code cache for javascript resources\n`);
  await TestRunner.loadModule('performance_test_runner');
  await TestRunner.showPanel('timeline');

  // Clear browser cache to avoid any existing entries for the fetched
  // scripts in the cache.
  SDK.multitargetNetworkManager.clearBrowserCache();

  await TestRunner.evaluateInPagePromise(`
      function loadScript() {
        const url = 'http://localhost:8000/devtools/resources/v8-cache-script.js';
        const frameId = 'frame_id';
        let iframeWindow = document.getElementById(frameId).contentWindow;
        return iframeWindow.loadScript(url)
            .then(() => iframeWindow.loadScript(url))
            .then(() => iframeWindow.loadScript(url));
      }
      function differentURLLoadScript() {
        const url = 'http://127.0.0.1:8000/devtools/resources/v8-cache-script.js';
        const frameId = 'diff_url_frame';
        let iframeWindow = document.getElementById(frameId).contentWindow;
        return iframeWindow.loadScript(url);
      }
      function sameURLLoadScript() {
        const url = 'http://localhost:8000/devtools/resources/v8-cache-script.js';
        const frameId = 'same_url_frame';
        let iframeWindow = document.getElementById(frameId).contentWindow;
        return iframeWindow.loadScript(url);
      }
  `);

  const scope = 'http://127.0.0.1:8000/devtools/service-workers/resources/v8-cache-iframe.html';
  const frameId = 'frame_id';
  const different_url_frameId = 'diff_url_frame';
  const same_url_frameId = 'same_url_frame';

  await TestRunner.addIframe(scope, {id: frameId});
  await TestRunner.addIframe(scope, {id: different_url_frameId});
  await TestRunner.addIframe(scope, {id: same_url_frameId});

  TestRunner.addResult('--- Trace events related to code caches ------');
  await PerformanceTestRunner.startTimeline();
  // This loads the same script thrice. With the current V8 heuristics (defined
  // in third_party/blink/renderer/bindings/core/v8/v8_code_cache.cc) we produce
  // cache on second fetch and consume it in the third fetch. This tests these
  // heuristics.
  await TestRunner.callFunctionInPageAsync('loadScript');

  // Load from a different origin should not use the cached code
  await TestRunner.callFunctionInPageAsync('differentURLLoadScript');

  // Loading from the same origin should use cached code
  await TestRunner.callFunctionInPageAsync('sameURLLoadScript');

  await PerformanceTestRunner.stopTimeline();
  PerformanceTestRunner.printTimelineRecordsWithDetails(
      TimelineModel.TimelineModel.RecordType.CompileScript);

  TestRunner.addResult('-----------------------------------------------');
  TestRunner.completeTest();
})();
