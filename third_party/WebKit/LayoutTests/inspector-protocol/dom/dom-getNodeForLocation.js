(async function(testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <div style='position:absolute;top:0;left:0;width:100;height:100'></div>
  `, 'Tests DOM.getNodeForLocation method.');
  var NodeTracker = await testRunner.loadScript('../resources/node-tracker.js');
  var nodeTracker = new NodeTracker(dp);

  dp.DOM.enable();
  var response = await dp.DOM.getNodeForLocation({x: 10, y: 10});
  var nodeId = response.result.nodeId;
  testRunner.logMessage(nodeTracker.nodeForId(nodeId), 'Node: ');
  testRunner.completeTest();
})

