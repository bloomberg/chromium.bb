(async function(testRunner) {
  var {page, session, dp} = await testRunner
      .startBlank('Tests ProcessInfo retrieval');

  response = await testRunner.browserP().SystemInfo.getProcessInfo();

  // The number of renderer processes varies, so log only the very first one.
  seenRenderer = false;
  for (process of response.result.processInfo) {
    if (process.type === "renderer") {
      if (!seenRenderer) {
        seenRenderer = true;
      } else {
        continue;
      }
    }
    testRunner.log(process, 'Process:', ['id', 'cpuTime']);
  }

  testRunner.completeTest();
})
