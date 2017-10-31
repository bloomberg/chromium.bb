(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that Page.lifecycleEvent is issued for important events.`);
  dp.Page.enable();
  dp.Runtime.enable();

  var events = [];
  dp.Page.onLifecycleEvent(result => {
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
