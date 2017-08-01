(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Test that all nodes from the detached DOM tree will get into one group in the heap snapshot. Bug 107819.`);

  await session.evaluate(`
    if (window.gc)
      window.gc();
    window.retaining_wrapper = document.createElement('div');
    var t = document.createElement('div');
    retaining_wrapper.appendChild(t);
    t.appendChild(document.createElement('div'));
  `);

  function checkDetachedDOMTreeNodes(treeNode) {
    var divCount = 0;
    for (var iter = treeNode.edges(); iter.hasNext(); iter.next()) {
      var node = iter.edge.node();
      if (node.name() === 'HTMLDivElement')
        ++divCount;
      else
        return testRunner.fail('unexpected DOM wrapper: ' + node.name());
    }
    if (divCount === 3)
      testRunner.log('SUCCESS: found ' + divCount + ' DIVs in ' + treeNode.name());
    else
      return testRunner.fail('unexpected DIV count: ' + divCount);
  }

  var Helper = await testRunner.loadScript('resources/heap-snapshot-common.js');
  var helper = await Helper(testRunner, session);

  var snapshot = await helper.takeHeapSnapshot();
  var node;
  for (var it = snapshot._allNodes(); it.hasNext(); it.next()) {
    if (it.node.name() === '(Detached DOM trees)') {
      node = it.node;
      break;
    }
  }
  if (node)
    testRunner.log('SUCCESS: found ' + node.name());
  else
    return testRunner.fail('cannot find detached DOM trees root');

  var detachedDOMTreeRE = /^Detached DOM tree/;
  var detachedDomTreeFound = false;
  for (var iter = node.edges(); iter.hasNext(); iter.next()) {
    var node = iter.edge.node();
    if (detachedDOMTreeRE.test(node.className())) {
      if ('Detached DOM tree / 3 entries' === node.name()) {
        if (detachedDomTreeFound)
          return testRunner.fail('second ' + node.name());
        detachedDomTreeFound = true;
        testRunner.log('SUCCESS: found ' + node.name());
        checkDetachedDOMTreeNodes(node);
      } else
        return testRunner.fail('unexpected detached DOM tree: ' + node.name());
    }
  }
  testRunner.completeTest();
})
