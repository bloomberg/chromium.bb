(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Test that all nodes from the detached DOM tree will get into one group in the heap snapshot. Bug 107819.`);

  await session.evaluate(`
    function addEventListenerAndRunTest() {
      function myEventListener(e) {
        console.log('myEventListener');
      }
      document.body.addEventListener('click', myEventListener, true);
    }
    addEventListenerAndRunTest();
  `);

  var Helper = await testRunner.loadScript('resources/heap-snapshot-common.js');
  var helper = await Helper(testRunner, session);

  var snapshot = await helper.takeHeapSnapshot();
  var node;
  for (var it = snapshot._allNodes(); it.hasNext(); it.next()) {
    if (it.node.type() === 'closure' && it.node.name() === 'myEventListener') {
      node = it.node;
      break;
    }
  }
  if (node)
    testRunner.log('SUCCESS: found ' + node.name());
  else
    return testRunner.fail('cannot find detached DOM trees root');

  var nativeRetainers = 0;
  for (var iter = node.retainers(); iter.hasNext(); iter.next()) {
    var retainingEdge = iter.retainer;
    if (retainingEdge.isInternal() && retainingEdge.name() === 'native') {
      if (++nativeRetainers === 1) {
        var retainerName = retainingEdge.node().name();
        if (retainerName === 'HTMLBodyElement')
          testRunner.log('SUCCESS: found link from HTMLBodyElement to ' + node.name());
        else
          return testRunner.fail('unexpected retainer of ' + node.name() + ': ' + retainerName);
      } else
        return testRunner.fail('too many retainers of ' + node.name());
    } else if (!retainingEdge.isWeak())
      return testRunner.fail('unexpected retaining edge of ' + node.name() + ' type: ' + retainingEdge.type() + ', name: ' + retainingEdge.name());
  }
  if (!nativeRetainers)
    return testRunner.fail('cannot find HTMLBodyElement among retainers of ' + node.name());
  testRunner.completeTest();
})
