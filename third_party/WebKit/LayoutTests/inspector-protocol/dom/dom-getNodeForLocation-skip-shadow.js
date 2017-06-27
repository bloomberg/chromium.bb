(async function(testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <form action='#'>
        <input type='text' style='position:absolute;top:0;left:0;width:100;height:100' />
    </form>
  `, '');

  var NodeTracker = await testRunner.loadScript('../resources/node-tracker.js');
  var nodeTracker = new NodeTracker(dp);

  dp.DOM.enable();
  var message = await dp.DOM.getNodeForLocation({x: 10, y: 10, includeUserAgentShadowDOM: false});
  var nodeId = message.result.nodeId;
  testRunner.logMessage(nodeTracker.nodeForId(nodeId), 'Node: ');
  testRunner.completeTest();
})
