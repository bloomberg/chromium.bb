(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that Page.lifecycleEvent is issued for important events.`);

  await dp.Page.enable();
  await dp.Page.setLifecycleEventsEnabled({ enabled: true });

  var events = [];
  var navigationLoaderId = null;
  dp.Page.onLifecycleEvent(event => {
    events.push(event);
    if (event.params.name === 'networkIdle' && navigationLoaderId && event.params.loaderId === navigationLoaderId) {
      var names = events.filter(event => event.params.loaderId === navigationLoaderId).map(event => event.params.name);
      names.sort();
      testRunner.log(names);
      testRunner.completeTest();
    }
  });

  var response = await dp.Page.navigate({url: "data:text/html,Hello!"});
  navigationLoaderId = response.result.loaderId;
})
