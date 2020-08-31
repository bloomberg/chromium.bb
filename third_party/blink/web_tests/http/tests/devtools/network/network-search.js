// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests search in network requests\n`);
  await TestRunner.loadModule('network_test_runner');
  await TestRunner.loadModule('console_test_runner');
  await TestRunner.showPanel('network');

  function initArgs(method, url, async, payload) {
    const args = {};
    args.method = method;
    args.url = url;
    args.async = async;
    args.payload = payload;
    const jsonArgs = JSON.stringify(args).replace(/\"/g, '\\"');
    return jsonArgs;
  }

  function printSubtree(treeElement) {
    let result = treeElement;
    for (const child of treeElement.children()) {
      TestRunner.addResult(`  ${child.listItemElement.textContent}`);
      treeElement = printSubtree(child);
    }
    return treeElement;
  }

  async function search(label, isRegex, ignoreCase, query = 'd.search') {
    TestRunner.addResult(label);
    const view = await Network.SearchNetworkView.openSearch(query);
    view._matchCaseButton.setToggled(!ignoreCase);
    view._regexButton.setToggled(isRegex);
    const promise = TestRunner.addSnifferPromise(view, '_searchFinished');
    view._onAction();
    await promise;
    const element = printSubtree(view._visiblePane._treeOutline.rootElement());
    TestRunner.addResult('');
    return element;
  }

  function networkItemSelected() {
    return new Promise(resolve => {
      function checkSelected() {
        if (UI.panels.network._networkLogView._dataGrid.selectedNode)
          resolve(UI.panels.network._networkLogView._dataGrid.selectedNode);
        else
          setTimeout(checkSelected, 0);
      }
      checkSelected();
    });
  }

  NetworkTestRunner.recordNetwork();

  let script = '';
  let count = 0;
  for (let i = 0; i < 10; i++) {
    count++;
    let suffix = i % 3 === 0 ? '-dosearch' : '';
    suffix += i % 2 === 0 ? '-doSearch' : '';
    suffix += i % 5 === 0 ? '-d.Search' : '';
    const args = initArgs('POST', `resources/echo-payload.php?n=${i}`, true, `request${i}${suffix}`)
    script += `makeXHRForJSONArguments("${args}");`;
  }
  ConsoleTestRunner.addConsoleSniffer(step2);
  TestRunner.evaluateInPage(script);

  async function step2(msg) {
    if (msg.messageText.indexOf('XHR loaded') === -1 || (--count)) {
      ConsoleTestRunner.addConsoleSniffer(step2);
      return;
    }
    await search('URL search', true, true, '8000/devtools');
    await search('Ignore case, regexp', true, true);
    await search('Ignore case, No regexp', false, true);
    const lastResult = await search('Case sensitive, regexp', true, false);

    TestRunner.addResult('Clicking on search result');
    const link = lastResult.listItemElement.getElementsByClassName('devtools-link')[0];
    link.click();
    const requestNode = await networkItemSelected();
    TestRunner.addResult(`Selected Node Name: ${requestNode._request.name().trimMiddle(100)}, URL: ${requestNode._request.url()}`);
    TestRunner.completeTest();
  }
})();
