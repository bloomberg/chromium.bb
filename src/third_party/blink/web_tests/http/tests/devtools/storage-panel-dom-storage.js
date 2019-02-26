// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Test that storage panel is present and that it contains correct data for local and session DOM storages.\n`);
  await TestRunner.loadModule('application_test_runner');
    // Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();

  await TestRunner.showPanel('resources');
  await TestRunner.evaluateInPagePromise(`
      function populateDOMStorage()
      {
          localStorage.clear();
          sessionStorage.clear();
          localStorage["resource-history"] = "{}"; // This mitigates file:// localstorage conflict.
          // Fill local and session storage with some random strings.
          for (var i = 0; i < 10; i++) {
              localStorage["a" + i] = "=value" + i + ", ";
              sessionStorage["b" + i] = "=value" + (i+10) + ", ";
          }
      }
  `);

  TestRunner.evaluateInPage('populateDOMStorage()', function(result) {
    TestRunner.addResult('Populated local and session storage');
  });
  function name(storage) {
    return storage.isLocalStorage ? 'Local storage' : 'Session storage';
  }

  function dumpDataGridContent(dataGrid) {
    var nodes = dataGrid.rootNode().children;
    var rows = [];
    for (var i = 0; i < nodes.length; ++i) {
      var node = nodes[i];
      if (!node.isCreationNode)
        rows.push(node._data.key + node._data.value);
    }
    rows.sort();
    TestRunner.addResult('KeyValue pairs: ' + rows.join(''));
  }

  function testStorageInView(storages) {
    var storage = storages.shift();
    if (!storage) {
      TestRunner.addResult('DONE');
      TestRunner.completeTest();
      return;
    }
    UI.panels.resources.showDOMStorage(storage);
    TestRunner.addResult('Did show: ' + name(storage));
    TestRunner.deprecatedRunAfterPendingDispatches(function() {
      TestRunner.addResult(name(storage) + ' content: ');
      var view = UI.panels.resources._domStorageView;
      dumpDataGridContent(view._dataGrid);
      TestRunner.deprecatedRunAfterPendingDispatches(() => testStorageInView(storages));
    });
  }

  TestRunner.deprecatedRunAfterPendingDispatches(function() {
    var storages = ApplicationTestRunner.domStorageModel().storages();
    if (storages)
      testStorageInView(storages.slice());
    else
      TestRunner.addResult('FAIL: no DOM storages found.');

  });
})();
