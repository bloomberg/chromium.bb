// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that oopif iframes are rendered inline.\n`);
  await TestRunner.loadModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('elements');

  // Save time on style updates.
  ElementsTestRunner.ignoreSidebarUpdates();

  await TestRunner.navigatePromise('resources/page-in.html');

  await ElementsTestRunner.expandAndDump();

  TestRunner.evaluateInPagePromise(`document.getElementById('page-iframe').src = 'http://devtools.oopif.test:8000/devtools/oopif/resources/inner-iframe.html';`);

  SDK.targetManager.observeTargets({
    targetAdded: async function(target) {
      target.model(SDK.ResourceTreeModel).agent.setLifecycleEventsEnabled(true);
      let loadedModels = 0;
      target.model(SDK.ResourceTreeModel).addEventListener(SDK.ResourceTreeModel.Events.LifecycleEvent, async (event) => {
        if (event.data.name === 'load') {
          loadedModels++;

          if (loadedModels >= 2) {
            ElementsTestRunner.expandElementsTree(async () => {
              // Because of the out-of-process component, there is a slight delay here
              // This requires expanding twice.
              await timeout(200);
              await ElementsTestRunner.expandAndDump();
              TestRunner.completeTest();
            });
          }
        }
      });
    },

    targetRemoved: function(target) {},
  });
})();

function timeout(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}
