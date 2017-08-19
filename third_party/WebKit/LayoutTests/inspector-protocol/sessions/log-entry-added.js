(async function(testRunner) {
  testRunner.log('Tests that multiple sessions receive log entries concurrently.');
  var page = await testRunner.createPage();
  var session1 = await page.createSession();
  var session2 = await page.createSession();

  session1.protocol.Log.onEntryAdded(event => {
    testRunner.log('From session1:');
    testRunner.logMessage(event);
  });
  session2.protocol.Log.onEntryAdded(event => {
    testRunner.log('From session2:');
    testRunner.logMessage(event);
  });

  testRunner.log('Enabling logging in session1');
  session1.protocol.Log.enable();
  session1.protocol.Log.startViolationsReport({config: [{name: 'discouragedAPIUse', threshold: -1}]});
  testRunner.log('Triggering violation');
  await session1.evaluate(`document.body.addEventListener('DOMSubtreeModified', () => {})`);

  testRunner.log('Enabling logging in session2');
  session2.protocol.Log.enable();
  session2.protocol.Log.startViolationsReport({config: [{name: 'discouragedAPIUse', threshold: -1}]});
  testRunner.log('Triggering violation');
  await session1.evaluate(`document.body.addEventListener('DOMSubtreeModified', () => {})`);

  testRunner.log('Disabling logging in session1');
  session1.protocol.Log.disable();
  testRunner.log('Triggering violation');
  await session1.evaluate(`document.body.addEventListener('DOMSubtreeModified', () => {})`);

  testRunner.log('Disabling logging in session2');
  session2.protocol.Log.disable();
  testRunner.log('Triggering violation');
  await session1.evaluate(`document.body.addEventListener('DOMSubtreeModified', () => {})`);

  testRunner.completeTest();
})
