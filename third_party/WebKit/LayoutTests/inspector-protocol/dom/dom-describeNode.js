(async function(testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <div id='div'><span>text</span></div>
  `, 'Tests DOM.desctibeNode method.');
  dp.Runtime.enable();
  var {result} = await dp.Runtime.evaluate({expression: `document.getElementById('div')`});
  var {result} = await dp.DOM.describeNode({objectId: result.result.objectId});
  testRunner.logObject(result.node, 'DIV', ['backendNodeId']);

  var {result} = await dp.Runtime.evaluate({expression: `document.body`});
  var {result} = await dp.DOM.describeNode({objectId: result.result.objectId});
  testRunner.logObject(result.node, 'BODY', ['backendNodeId']);

  var {result} = await dp.Runtime.evaluate({expression: `document.body`});
  var {result} = await dp.DOM.describeNode({objectId: result.result.objectId, depth: -1});
  testRunner.logObject(result.node, 'BODY DEEP', ['backendNodeId']);

  testRunner.completeTest();
})
