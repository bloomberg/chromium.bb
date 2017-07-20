(async function(testRunner) {
  let {page, session, dp} = await testRunner.startBlank('Tests that dom modification event listener produces a violation.');
  dp.Log.onEntryAdded(testRunner.logMessage.bind(testRunner));
  dp.Log.enable();
  dp.Log.startViolationsReport({config: [{name: 'discouragedAPIUse', threshold: -1}]});
  await session.evaluate(`document.body.addEventListener('DOMSubtreeModified', () => {})`);
  testRunner.completeTest();
})
