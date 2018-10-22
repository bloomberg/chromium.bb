(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests that Page.addScriptToEvaluateOnLoad is executed in the order of addition');
  dp.Runtime.enable();
  dp.Page.enable();

  dp.Runtime.onConsoleAPICalled(msg => testRunner.log(msg.params.args[0].value));

  for (let i = 0; i < 15; ++i) {
    await dp.Page.addScriptToEvaluateOnNewDocument({source: `
      console.log('message from ${i}');
    `});
  }
  await session.navigate('../resources/blank.html');
  testRunner.completeTest();
})
