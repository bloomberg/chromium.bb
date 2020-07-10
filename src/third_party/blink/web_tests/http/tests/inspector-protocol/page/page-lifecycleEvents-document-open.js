(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that Page.lifecycleEvent is issued for important events.`);

  await dp.Page.enable();
  await dp.Page.setLifecycleEventsEnabled({ enabled: true });

  var events = [];
  dp.Page.onLifecycleEvent(event => {
    // Filter out firstMeaningfulPaint and friends.
    if (event.params.name.startsWith('first'))
      return;
    events.push(event);
    if (event.params.name === 'networkIdle') {
      var names = events.map(event => event.params.name);
      testRunner.log(names);
      testRunner.completeTest();
    }
  });

  var response = await session.evaluate(`
    document.open();
    document.write('Hello, world');
    document.close();
  `);
})
