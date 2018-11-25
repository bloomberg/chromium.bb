// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that the changes sidebar contains the changed uisourcecodes.\n`);
  await TestRunner.loadModule('changes');

  var fulfill = function() {};
  var workspace = new Workspace.Workspace();
  var project =
      new Bindings.ContentProviderBasedProject(workspace, 'mockProject', Workspace.projectTypes.Network, '', false);
  var workspaceDiff = new WorkspaceDiff.WorkspaceDiff(workspace);
  TestRunner.addSniffer(
      WorkspaceDiff.WorkspaceDiff.prototype, '_uiSourceCodeProcessedForTest', modifiedStatusChanged, true);

  var uiSourceCodeList = new Changes.ChangesSidebar(workspaceDiff);

  var firstUISC = addUISourceCode('first.css', '.first {color: red}');
  var secondUISC = addUISourceCode('second.css', '.second {color: red}');
  var thirdUISC = addUISourceCode('third.css', '.third {color: red}');
  uiSourceCodeList.show(UI.inspectorView.element);

  TestRunner.runTestSuite([
    function initialState(next) {
      dumpAfterLoadingFinished().then(next);
    },
    function workingCopyChanged(next) {
      firstUISC.setWorkingCopy('.first {color: blue}');
      dumpAfterLoadingFinished().then(next);
    },
    function workingCopyComitted(next) {
      firstUISC.commitWorkingCopy();
      secondUISC.addRevision('.second {color: blue}');
      dumpAfterLoadingFinished().then(next);
    },
    function resetAll(next) {
      firstUISC.addRevision('.first {color: red}');
      secondUISC.addRevision('.second {color: red}');
      thirdUISC.addRevision('.third {color: red}');
      dumpAfterLoadingFinished().then(next);
    }

  ]);

  function modifiedStatusChanged() {
    if (!workspaceDiff._loadingUISourceCodes.size)
      fulfill();
  }

  function dumpUISourceCodeList() {
    uiSourceCodeList._treeoutline.rootElement().children().forEach(treeElement => {
      TestRunner.addResult(treeElement.title);
    });
  }

  function dumpAfterLoadingFinished() {
    var promise = new Promise(x => fulfill = x);
    modifiedStatusChanged();
    return promise.then(dumpUISourceCodeList);
  }

  function addUISourceCode(url, content) {
    return project.addContentProvider(
        url, Common.StaticContentProvider.fromString(url, Common.resourceTypes.Stylesheet, content));
  }
})();
