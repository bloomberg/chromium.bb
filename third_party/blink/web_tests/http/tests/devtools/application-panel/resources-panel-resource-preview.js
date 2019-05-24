// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests Application Panel preview for resources of different types.\n`);
  await TestRunner.loadModule('application_test_runner');
    // Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();

  await TestRunner.loadModule('console_test_runner');
  await TestRunner.showPanel('resources');
  await TestRunner.loadHTML(`
      <img src="../resources/image.png">
    `);
  await TestRunner.evaluateInPagePromise(`
      function parse(val) {
          // This is here for the JSON file imported via the script tag below
      }
    `);
  await TestRunner.addScriptTag('../resources/json-value.js');
  await TestRunner.addScriptTag('./resources-panel-resource-preview.js');

  await UI.viewManager.showView('resources');
  ApplicationTestRunner.dumpCurrentState('Initial state:');
  await ApplicationTestRunner.revealResourceWithDisplayName('json-value.js');
  await ApplicationTestRunner.revealResourceWithDisplayName('image.png');
  await ApplicationTestRunner.revealResourceWithDisplayName('resources-panel-resource-preview.js');

  TestRunner.completeTest();
})();
