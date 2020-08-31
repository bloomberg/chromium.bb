// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that keyframes are shown in styles pane.\n`);
  await TestRunner.loadModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      @keyframes animName {
          from, 20% {
              margin-left: 200px;
              color: red;
          }
          to {
              margin-left: 500px;
          }
      }
      </style>
      <div id="element"></div>
    `);
  await TestRunner.addStylesheetTag('../styles/resources/keyframes.css');

  ElementsTestRunner.selectNodeAndWaitForStyles('element', step1);

  async function step1() {
    TestRunner.addResult('=== Before key modification ===');
    await ElementsTestRunner.dumpSelectedElementStyles(true);
    var section = UI.panels.elements._stylesWidget._sectionBlocks[1].sections[1];
    section.startEditingSelector();
    section._selectorElement.textContent = '1%';
    section._selectorElement.dispatchEvent(TestRunner.createKeyEvent('Enter'));
    ElementsTestRunner.waitForSelectorCommitted(step2);
  }

  async function step2() {
    TestRunner.addResult('=== After key modification ===');
    await ElementsTestRunner.dumpSelectedElementStyles(true);
    SDK.domModelUndoStack.undo();
    ElementsTestRunner.waitForStyles('element', step3, true);
  }

  async function step3() {
    TestRunner.addResult('=== After undo ===');
    await ElementsTestRunner.dumpSelectedElementStyles(true);

    SDK.domModelUndoStack.redo();
    ElementsTestRunner.waitForStyles('element', step4, true);
  }

  async function step4() {
    TestRunner.addResult('=== After redo ===');
    await ElementsTestRunner.dumpSelectedElementStyles(true);
    var section = UI.panels.elements._stylesWidget._sectionBlocks[1].sections[1];
    section.startEditingSelector();
    section._selectorElement.textContent = '1% /*';
    section._selectorElement.dispatchEvent(TestRunner.createKeyEvent('Enter'));
    ElementsTestRunner.waitForSelectorCommitted(step5);
  }

  async function step5() {
    TestRunner.addResult('=== After invalid key modification ===');
    await ElementsTestRunner.dumpSelectedElementStyles(true);
    TestRunner.completeTest();
  }
})();
