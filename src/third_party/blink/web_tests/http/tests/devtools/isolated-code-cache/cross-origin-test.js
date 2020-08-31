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

  await TestRunner.navigatePromise(
      'http://127.0.0.1:8000/devtools/resources/test-page-v8-code-cache.html');

  await TestRunner.evaluateInPagePromise(`
      function loadScript() {
        const url =
            'http://localhost:8000/devtools/resources/v8-cache-script.js';
        const frameId = 'frame_id';
        let iframeWindow = document.getElementById(frameId).contentWindow;
        return iframeWindow.loadScript(url)
            .then(() => iframeWindow.loadScript(url))
            .then(() => iframeWindow.loadScript(url));
      }
  `);

  const scope =
      'http://127.0.0.1:8000/devtools/service-workers/resources/v8-cache-iframe.html';
  const frameId = 'frame_id';

  await TestRunner.addIframe(scope, {id: frameId});

  TestRunner.addResult(
      '---First navigation - produce and consume code cache ------\n');

  // This loads the same script thrice. With the current V8 heuristics (defined
  // in third_party/blink/renderer/bindings/core/v8/v8_code_cache.cc) we produce
  // cache on second fetch and consume it in the third fetch. We may have to
  // change this if the heuristics change.
  await PerformanceTestRunner.invokeAsyncWithTimeline('loadScript');
  await PerformanceTestRunner.printTimelineRecordsWithDetails(
      TimelineModel.TimelineModel.RecordType.CompileScript);

  // Second navigation
  TestRunner.addResult(
      '\n--- Second navigation - from a different origin ------\n');

  await TestRunner.navigatePromise(
      'http://localhost:8000/devtools/resources/test-page-v8-code-cache.html');
  await TestRunner.evaluateInPagePromise(`
      function loadScript() {
        const url =
            'http://localhost:8000/devtools/resources/v8-cache-script.js';
        const frameId = 'frame_id';
        let iframeWindow = document.getElementById(frameId).contentWindow;
        return iframeWindow.loadScript(url);
      }
  `);

  const localhost_scope =
      'http://localhost:8000/devtools/service-workers/resources/v8-cache-iframe.html';

  await TestRunner.addIframe(localhost_scope, {id: frameId});
  await PerformanceTestRunner.invokeAsyncWithTimeline('loadScript');
  await PerformanceTestRunner.printTimelineRecordsWithDetails(
      TimelineModel.TimelineModel.RecordType.CompileScript);

  TestRunner.completeTest();
})();
