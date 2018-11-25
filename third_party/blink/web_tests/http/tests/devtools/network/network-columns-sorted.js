// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests network columns are sortable.\n`);
  await TestRunner.loadModule('network_test_runner');
  await TestRunner.showPanel('network');

  NetworkTestRunner.recordNetwork();

  var totalResourceCount = 4;
  function fetchRequests() {
    NetworkTestRunner.makeFetch('resources/style.css', {}, ensureAllResources);
    NetworkTestRunner.makeFetch('resources/abe.png', {}, () => {
      // Ensures result is cached.
      NetworkTestRunner.makeFetch('resources/abe.png', {}, ensureAllResources);
      ensureAllResources();
    });
    NetworkTestRunner.makeFetch('missing/foo.bar', {}, ensureAllResources);
  }

  var resourceCount = 0;
  function ensureAllResources() {
    if (++resourceCount < totalResourceCount)
      return;
    UI.panels.network._networkLogView._refresh();
    sortGrid();
    TestRunner.completeTest();
  }

  function sortGrid() {
    var logView = UI.panels.network._networkLogView;
    var dataGrid = logView._dataGrid;
    var columnsView = logView._columns;
    TestRunner.addSniffer(columnsView, '_dataGridSortedForTest', dataGridSorted.bind(null, logView), true);

    dataGrid.markColumnAsSortedBy('name', DataGrid.DataGrid.Order.Ascending);
    columnsView.sortByCurrentColumn();
    dataGrid.markColumnAsSortedBy('name', DataGrid.DataGrid.Order.Descending);
    columnsView.sortByCurrentColumn();
  }

  function dataGridSorted(logView) {
    var nodes = logView._dataGrid.rootNode().flatChildren();
    TestRunner.addResult('Sorted by: ' + logView._dataGrid.sortColumnId());
    for (var node of nodes)
      TestRunner.addResult('    ' + node.request().name());
    TestRunner.addResult('');
  }
  fetchRequests();
})();
