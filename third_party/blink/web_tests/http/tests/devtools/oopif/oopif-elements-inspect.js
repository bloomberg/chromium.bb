// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that inspect request works for nested OOPIF elements.\n`);
  await TestRunner.loadModule('elements_test_runner');
  await TestRunner.showPanel('elements');

  // Save time on style updates.
  Elements.StylesSidebarPane.prototype.update = function() {};
  Elements.MetricsSidebarPane.prototype.update = function() {};

  TestRunner.navigatePromise('resources/page-inspect.html');

  SDK.targetManager.observeTargets({
    targetAdded: async function(target) {
      if (target.name() === 'Main')
        return;
      let complete = false;
      target.pageAgent().setLifecycleEventsEnabled(true);
      target.model(SDK.ResourceTreeModel).addEventListener(SDK.ResourceTreeModel.Events.LifecycleEvent, async (event) => {
        if (event.data.name === 'load' && !complete) {
          complete = true;

          target.model(SDK.RuntimeModel).defaultExecutionContext().evaluate({
            expression: 'inspect(document.body)',
            includeCommandLineAPI: true
          }, false, false);

          UI.context.addFlavorChangeListener(SDK.DOMNode, (event) => {
            const treeOutline = Elements.ElementsTreeOutline.forDOMModel(event.data.domModel());
            TestRunner.addResult(`Selected node has text: ${treeOutline.selectedDOMNode().children()[0].nodeName()}`);
            TestRunner.completeTest();
          });
        }
      });
    },

    targetRemoved: function(target) {},
  });

})();
