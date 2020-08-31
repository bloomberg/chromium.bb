// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  'use strict';
  TestRunner.addResult(`Tests that Elements properly populate and select after immediate updates crbug.com/829884\n`);
  await TestRunner.loadModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <!DOCTYPE HTML">
      <html>
      <head></head>
      <body id="body">
        <div>FooBar1</div>
        <div>FooBar2</div>
      </body>
      </html>
    `);

  const treeOutline = ElementsTestRunner.firstElementsTreeOutline();
  ElementsTestRunner.selectNodeWithId('body', node => {
    TestRunner.addResult(`BEFORE: children: ${node.children()}, childNodeCount: ${node.childNodeCount()}`);

    // Any operation that modifies the node, followed by an immediate, synchronous update.
    TestRunner.domModel._childNodeCountUpdated(node.id, 3);
    treeOutline._updateModifiedNodes();

    TestRunner.addResult(`AFTER: children: ${node.children()}, childNodeCount: ${node.childNodeCount()}`);
    ElementsTestRunner.expandElementsTree(afterExpand);
  });

  function afterExpand() {
    ElementsTestRunner.selectNodeWithId('body', node => {
      const treeElement = node[treeOutline.treeElementSymbol()];
      TestRunner.addResult(`AFTER EXPAND: TreeElement childCount: ${treeElement.childCount()}`);

      var selectedElement = treeOutline.selectedTreeElement;
      var nodeName = selectedElement ? selectedElement.node().nodeName() : 'null';
      TestRunner.addResult('Selected element:\'' + nodeName + '\'');
      TestRunner.completeTest();
    });
  }
})();
