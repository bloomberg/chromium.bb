(async function(testRunner) {
  let {page, session, dp} = await testRunner.startBlank(``);

  dp.Runtime.enable();

  dp.Runtime.evaluate({ expression: 'console.log(239)' });
  testRunner.logMessage(await dp.Runtime.onceConsoleAPICalled());
  dp.Runtime.evaluate({ expression: 'var l = console.log;\n  l(239)' });
  testRunner.logMessage(await dp.Runtime.onceConsoleAPICalled());
  testRunner.completeTest();
})
