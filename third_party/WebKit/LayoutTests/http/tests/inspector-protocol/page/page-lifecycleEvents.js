(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that Page.lifecycleEvent is issued for important events.`);

  await dp.Page.enable();

  var [lifecycleEvent] = await Promise.all([
    dp.Page.onceLifecycleEvent(),
    dp.Page.setLifecycleEventsEnabled({ enabled: true })
  ]);
  var loaderId = lifecycleEvent.params.loaderId;

  var events = [];
  dp.Page.onLifecycleEvent(result => {
    // Navigation results in lifecycle events with a new loaderId.
    if (result.params.loaderId === loaderId)
      return;
    events.push(result.params.name);
    if (events.includes('networkIdle')) {
      // 'load' can come before 'DOMContentLoaded'
      events.sort();
      testRunner.log(events);
      testRunner.completeTest();
    }
  });

  dp.Page.navigate({url: "data:text/html,Hello!"});

})
