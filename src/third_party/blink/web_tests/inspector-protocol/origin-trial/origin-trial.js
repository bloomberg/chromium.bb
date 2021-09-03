(async function(testRunner) {
  const {page, session, dp} = await testRunner.startHTML(`
  <meta http-equiv="origin-trial"
        content="AnwB3aSh6U8pmYtO/AzzxELSwk8BRJoj77nUnCq6u3N8LMJKlX/ImydQmXn3SgE0a+8RyowLbV835tXQHJMHuAEAAABQeyJvcmlnaW4iOiAiaHR0cDovLzEyNy4wLjAuMTo4MDAwIiwgImZlYXR1cmUiOiAiQXBwQ2FjaGUiLCAiZXhwaXJ5IjogMTc2MTE3NjE5OH0="
  />
  `, 'Verifies that we can successfully retrieve origin trial in frame tree.');

  await dp.Page.enable();
  testRunner.log(
    (await dp.Page.getResourceTree()).result.frameTree.frame.originTrials);
  testRunner.completeTest();
});