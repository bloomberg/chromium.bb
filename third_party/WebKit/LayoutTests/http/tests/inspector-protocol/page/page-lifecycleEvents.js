(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that Page.lifecycleEvent is issued for important events.`);
  dp.Page.enable();
  dp.Runtime.enable();

  var events = [];
  dp.Page.onLifecycleEvent(result => {
    events.push(result.params.name);
    if (events.includes('networkIdle') && events.includes('networkAlmostIdle')) {
      events.sort();
      testRunner.log(events);
      testRunner.completeTest();
    }
  });

  // It's possible for Blink to finish the load and run out of tasks before
  // network idle lifecycle events are generated.  Add a timeout
  // greater than the network quiet window to guarantee that those events fire.
  dp.Page.navigate({url: "data:text/html,Hello! <script>setTimeout(() => {}, 2000);</script>"});
})
