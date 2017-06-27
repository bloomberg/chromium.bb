(async function(testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    Several<br>
    Lines<br>
    Of<br>
    Text<br>
    <div style='position:absolute;top:100;left:0;width:100;height:100;background:red'></div>
    <div style='position:absolute;top:200;left:100;width:100;height:100;background:green'></div>
    <div style='position:absolute;top:150;left:50;width:100;height:100;background:blue;transform:rotate(45deg);'></div>
  `, '');
  var NodeTracker = await testRunner.loadScript('../resources/node-tracker.js');
  var nodeTracker = new NodeTracker(dp);
  dp.DOM.enable();
  await dp.DOM.getNodeForLocation({x: 100, y: 200});

  for (var nodeId of nodeTracker.nodeIds()) {
    var message = await dp.DOM.getBoxModel({nodeId});
    var node = nodeTracker.nodeForId(nodeId);
    if (message.error)
      testRunner.log(node.nodeName + ': ' + message.error.message);
    else
      testRunner.logObject(message.result.model.content, node.nodeName + ' ' + node.attributes + ' ');
  }
  testRunner.completeTest();
})

