// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests file system project mappings.\n`);
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.loadModule('elements_test_runner');
  await TestRunner.loadModule('bindings_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>#inspected {
          color: red;
      }
      /*# sourceURL=http://localhost:8000/inspector/elements/styles/foo.css */
      </style>
      <div id="inspected"></div>
    `);

  TestRunner.markStep('testEditingRulesInElementsPanelDoesNotAddSourceURLToOriginalFile');

  var uiSourceCode = await TestRunner.waitForUISourceCode('foo.css');
  await uiSourceCode.requestContent();
  TestRunner.addResult('Dumping uiSourceCode content:');
  TestRunner.addResult(uiSourceCode.workingCopy());
  ElementsTestRunner.selectNodeAndWaitForStyles('inspected', nodeSelected);

  var uiSourceCode;

  async function nodeSelected() {
    TestRunner.addResult('Dumping matched rules:');
    await ElementsTestRunner.dumpSelectedElementStyles(true);
    TestRunner.addResult('Editing styles from elements panel:');
    var treeElement = ElementsTestRunner.getMatchedStylePropertyTreeItem('color');
    treeElement.startEditing();
    treeElement.nameElement.textContent = 'color';
    treeElement.nameElement.dispatchEvent(TestRunner.createKeyEvent('Enter'));

    // Commit editing.
    treeElement.valueElement.textContent = 'green';
    treeElement.valueElement.firstChild.select();
    treeElement.valueElement.dispatchEvent(TestRunner.createKeyEvent('Enter'));
    uiSourceCode.addEventListener(Workspace.UISourceCode.Events.WorkingCopyCommitted, stylesEdited, this);
  }

  async function stylesEdited() {
    TestRunner.addResult('Styles edited.');
    TestRunner.addResult('Dumping matched rules:');
    await ElementsTestRunner.dumpSelectedElementStyles(true);
    TestRunner.addResult('Dumping uiSourceCode content:');
    TestRunner.addResult(uiSourceCode.workingCopy());
    TestRunner.completeTest();
  }
})();
