(async function(testRunner) {
  var {page, session, dp} = await testRunner.startURL('../resources/dom-snapshot.html', 'Tests DOMSnapshot.getSnapshot method.');

  await session.evaluate(`
    var host = document.querySelector('#shadow-host').createShadowRoot();
    var template = document.querySelector('#shadow-template');
    host.appendChild(template.content);
    template.remove();
    document.body.offsetWidth;
  `);

  function stabilize(key, value) {
    var unstableKeys = ['documentURL', 'baseURL', 'frameId', 'backendNodeId'];
    if (unstableKeys.indexOf(key) !== -1)
      return '<' + typeof(value) + '>';
    if (typeof value === 'string' && value.indexOf('/dom-snapshot/') !== -1)
      value = '<value>';
    return value;
  }

  var whitelist = ['transform', 'transform-origin', 'height', 'width', 'display', 'outline-color', 'color'];
  var response = await dp.DOMSnapshot.getSnapshot({'computedStyleWhitelist': whitelist});
  if (response.error)
    testRunner.log(response);
  else
    testRunner.log(JSON.stringify(response.result, stabilize, 2));
  testRunner.completeTest();
})
