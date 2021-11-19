// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests synchronizing the search input field to the editor selection.\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.loadLegacyModule('search');
  await TestRunner.showPanel('sources');
  await TestRunner.addScriptTag('../sources/debugger/resources/edit-me.js');

  var panel = UI.panels.sources;

  SourcesTestRunner.showScriptSource('edit-me.js', step1);

  function step1(sourceFrame) {
    sourceFrame.textEditor.setSelection(findString(sourceFrame, 'return'));
    setTimeout(step2);
  }

  async function step2() {
    panel.searchableView().showSearchField();
    TestRunner.addResult('Search controller: \'' + panel.searchableView().searchInputElement.value + '\'');
    var action = new Sources.SearchSourcesView.ActionDelegate();
    await action.showSearch();
    var searchView = /** @type {!Search.SearchView} */ (
        Sources.SearchSourcesView.instance());
    TestRunner.addResult('Advanced search controller: \'' + searchView.search.value + '\'');
    TestRunner.completeTest();
  }

  function findString(sourceFrame, string) {
    for (var i = 0; i < sourceFrame.textEditor.linesCount; ++i) {
      var line = sourceFrame.textEditor.line(i);
      var column = line.indexOf(string);
      if (column === -1)
        continue;
      return new TextUtils.TextRange(i, column, i, column + string.length);
    }
  }
})();
