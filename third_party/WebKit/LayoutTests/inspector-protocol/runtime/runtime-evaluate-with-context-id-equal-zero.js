(async function(testRunner) {
  let {page, session, dp} = await testRunner.startBlank(`Tests that DevTools doesn't crash on Runtime.evaluate with contextId equals 0.`);
  testRunner.logMessage(await dp.Runtime.evaluate({contextId: 0, expression: '' }));
  testRunner.completeTest();
})
