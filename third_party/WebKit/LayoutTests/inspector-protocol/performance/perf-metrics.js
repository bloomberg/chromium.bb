(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Test that page performance metrics are retrieved and the list is stable.\n` +
      `DO NOT modify the list, it's exposed over public protocol.`);

  await dumpMetrics();
  await dp.Performance.enable();
  await dumpMetrics();
  await dp.Performance.disable();
  await dumpMetrics();

  async function retrieveMetrics() {
    const {result:{metrics}} = await dp.Performance.getMetrics();
    const map = new Map();
    for (const metric of metrics)
      map.set(metric.name, metric.value);
    return map;
  }

  async function dumpMetrics() {
    const metrics = await retrieveMetrics();
    const metricsToCheck = new Set([
      'Timestamp',
      'Documents',
      'Frames',
      'JSEventListeners',
      'Nodes',
      'LayoutCount',
      'RecalcStyleCount',
      'LayoutDuration',
      'RecalcStyleDuration',
      'ScriptDuration',
      'TaskDuration',
      'JSHeapUsedSize',
      'JSHeapTotalSize',
    ]);

    testRunner.log('\nMetrics:');
    testRunner.log(Array.from(metrics.keys()).filter(n => metricsToCheck.has(n)).sort().join('\n'));

    checkMetric('Documents');
    checkMetric('Nodes');
    checkMetric('JSHeapUsedSize');
    checkMetric('JSHeapTotalSize');

    function checkMetric(name) {
      if (metrics.size && !metrics.get(name))
        testRunner.log(`Error: Metric ${name} has a bad value ${metrics.get(name)}`);
    }
  }

  testRunner.completeTest();

})
