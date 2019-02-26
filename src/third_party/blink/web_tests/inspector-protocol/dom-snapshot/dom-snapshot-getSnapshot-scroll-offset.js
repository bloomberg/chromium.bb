(async function(testRunner) {
  var {page, session, dp} = await testRunner.startURL('../resources/dom-snapshot-scroll-offset.html', 'Tests DOMSnapshot.getSnapshot reports scroll offset and bounding box in terms of document coordinates.');

  function stabilize(key, value) {
    var unstableKeys = ['documentURL', 'baseURL', 'frameId', 'backendNodeId'];
    if (unstableKeys.indexOf(key) !== -1)
      return '<' + typeof(value) + '>';
    return value;
  }

  var response = await dp.DOMSnapshot.getSnapshot({'computedStyleWhitelist': []});
  if (response.error)
    testRunner.log(response);
  else
    testRunner.log(JSON.stringify(response.result, stabilize, 2));
  testRunner.completeTest();
})
