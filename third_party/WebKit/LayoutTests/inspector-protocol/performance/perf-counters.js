(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      'Test that page performance metrics are retrieved.');

  await dumpMetrics();
  await dp.Performance.enable();
  await dumpMetrics();
  await dumpMetrics();
  await dp.Performance.disable();
  await dumpMetrics();

  async function dumpMetrics() {
    const {result:{metrics}} = await dp.Performance.getMetrics();
    testRunner.log(JSON.stringify(metrics.map(metric => metric.name)));
  }

  testRunner.completeTest();
})
