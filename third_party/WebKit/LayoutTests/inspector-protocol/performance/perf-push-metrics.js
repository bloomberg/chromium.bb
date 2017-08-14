(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Test that console.timeStamp generates Performance.metrics event');

  const responses = new Promise(resolve => {
    let count = 3;
    dp.Performance.onMetrics(response => {
      const params = response.params;
      testRunner.log(`title: ${params.title}`);
      testRunner.log(`metrics: ${params.metrics.map(metric => metric.name).join(', ')}`);
      if (!--count)
        resolve();
    });
  });

  await dp.Performance.enable();
  await session.evaluate(`console.timeStamp('test1')`);
  await session.evaluate(`console.timeStamp('test1')`);
  await session.evaluate(`console.timeStamp()`);
  await responses;
  await dp.Performance.disable();

  testRunner.completeTest();
})
