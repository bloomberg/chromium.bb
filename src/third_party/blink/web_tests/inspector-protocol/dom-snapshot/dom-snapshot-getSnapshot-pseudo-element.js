(async function(testRunner) {
  var {page, session, dp} = await testRunner.startURL('../resources/dom-snapshot-pseudo-element.html', 'Tests DOMSnapshot.getSnapshot exports layout tree nodes associated with pseudo elements.');

  function stabilize(key, value) {
    var unstableKeys = ['documentURL', 'baseURL', 'frameId', 'backendNodeId'];
    if (unstableKeys.indexOf(key) !== -1)
      return '<' + typeof(value) + '>';
    return value;
  }

  var response = await dp.DOMSnapshot.getSnapshot({'computedStyleWhitelist': ['font-weight', 'color'], 'includeEventListeners': true});
  if (response.error)
    testRunner.log(response);
  else
    testRunner.log(JSON.stringify(response.result, stabilize, 2));
  testRunner.completeTest();
})
