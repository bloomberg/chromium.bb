// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests list of performance metrics and that memory is not double counted.\n`);

  const model = SDK.targetManager.mainTarget().model(SDK.PerformanceMetricsModel);
  await model.enable();
  let metrics = (await model.requestMetrics()).metrics;

  TestRunner.addResult('\nMetrics reported:');
  TestRunner.addResults(metrics.keysArray().sort());

  await TestRunner.navigatePromise('resources/page.html');
  TestRunner.addResult('\nTargets after navigate');
  TestRunner.addResults(SDK.targetManager.targets().map(t => t.name()).sort());

  let lastTotal, total;
  do {
    metrics = (await model.requestMetrics()).metrics;
    lastTotal = total;
    total = 0;
    for (const m of SDK.targetManager.models(SDK.RuntimeModel))
      total += (await m.heapUsage()).totalSize;
  } while (total !== lastTotal);

  TestRunner.addResult('\nmetrics size is twice smaller than simple sum: ' +
      (metrics.get('JSHeapTotalSize') * 2 === total));

  TestRunner.completeTest();
})();
