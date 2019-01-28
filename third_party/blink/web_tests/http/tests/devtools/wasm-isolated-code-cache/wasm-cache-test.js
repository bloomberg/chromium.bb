// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests V8 code cache for WebAssembly resources.\n`);
  await TestRunner.loadModule('performance_test_runner');
  await TestRunner.showPanel('timeline');

  // Clear browser cache to avoid any existing entries for the fetched
  // scripts in the cache.
  SDK.multitargetNetworkManager.clearBrowserCache();

  function runTests() {
    // Loads a WASM module that is smaller than the threshold twice. It should
    // compile and not be cached.
    function loadSmallWasmModule(iframe_window) {
      const url = 'http://127.0.0.1:8000/wasm/resources/load-wasm.php?name=small.wasm&cors'
      return iframe_window.instantiateModule(url)
        .then(() => iframe_window.instantiateModule(url));
    }
    // Loads a WASM module that is larger than the caching threshold. It
    // should be cached the first run and bypass compilation the second.
    function loadLargeWasmModule(iframe_window) {
      const url = 'http://127.0.0.1:8000/wasm/resources/load-wasm.php?name=large.wasm&cors'
      return iframe_window.instantiateModule(url)
        .then(() => iframe_window.instantiateModule(url));
    }
    // Load the same large WASM module with a different URL. It should miss
    // the cache, compile and be cached.
    function loadOtherLargeWasmModule(iframe_window) {
      const url = 'http://localhost:8000/wasm/resources/load-wasm.php?name=large.wasm&cors'
      return iframe_window.instantiateModule(url);
    }

    let script = document.createElement('script');
    script.type = 'module';
    script.text = 'window.finishTest()';
    document.body.appendChild(script);

    const frameId = 'frame_id';
    const iframe_window = document.getElementById(frameId).contentWindow;

    // These functions must be called in this order.
    return loadSmallWasmModule(iframe_window)
      .then(() => loadLargeWasmModule(iframe_window))
      .then(() => loadOtherLargeWasmModule(iframe_window));
  }

  await TestRunner.evaluateInPagePromise(runTests.toString());

  TestRunner.addResult(
      '---First navigation - produce and consume code cache ------\n');

  // Create a same origin iframe.
  const scope = 'http://127.0.0.1:8000/wasm/resources/wasm-cache-iframe.html';
  await TestRunner.addIframe(scope, {id: 'frame_id'});

  await PerformanceTestRunner.invokeAsyncWithTimeline('runTests');

  const events = new Set([
    TimelineModel.TimelineModel.RecordType.WasmStreamFromResponseCallback,
    TimelineModel.TimelineModel.RecordType.WasmCompiledModule,
    TimelineModel.TimelineModel.RecordType.WasmCachedModule,
    TimelineModel.TimelineModel.RecordType.WasmModuleCacheHit,
    TimelineModel.TimelineModel.RecordType.WasmModuleCacheInvalid]);
  const tracingModel = PerformanceTestRunner.tracingModel();

  tracingModel.sortedProcesses().forEach(p => p.sortedThreads().forEach(t =>
      t.events().filter(event => events.has(event.name)).forEach(PerformanceTestRunner.printTraceEventProperties)));

  // Second navigation
  TestRunner.addResult(
      '\n--- Second navigation - from a different origin ------\n');

  const other_scope = 'http://localhost:8000/wasm/resources/wasm-cache-iframe.html';
  await TestRunner.addIframe(other_scope, {id: 'frame_id'});

  await PerformanceTestRunner.invokeAsyncWithTimeline('runTests');

  tracingModel.sortedProcesses().forEach(p => p.sortedThreads().forEach(t =>
      t.events().filter(event => events.has(event.name)).forEach(PerformanceTestRunner.printTraceEventProperties)));

  TestRunner.completeTest();
})();
