(async function(testRunner) {
  let {page, session, dp} = await testRunner.startBlank(`Tests that default execution context accessed without enabling Runtime domain gets properly cleaned up on reload.`);
  await session.evaluate('window.dummyObject = { a : 1 };');
  var result = await dp.Runtime.evaluate({expression: 'window.dummyObject' });
  await dp.Page.reload();
  testRunner.logMessage(await dp.Runtime.getProperties({ objectId: result.result.result.objectId, ownProperties: true }));
  testRunner.completeTest();
})
