// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function () {
  TestRunner.addResult(`Tests accessibility in the preview view for JSON and images using the axe-core linter.\n`);
  await TestRunner.loadModule('application_test_runner');
  await TestRunner.loadModule('axe_core_test_runner');

  // Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();
  await TestRunner.loadModule('console_test_runner');
  await TestRunner.showPanel('resources');

  ApplicationTestRunner.dumpCurrentState('Initial state:');

  const tests = [testJSONView, testImageView];

  async function testJSONView() {
    await TestRunner.evaluateInPagePromise(`
        function parse(val) {
            // This is here for the JSON file imported via the script tag below
        }
      `);
    await TestRunner.addScriptTag('../../resources/json-value.js');
    await ApplicationTestRunner.revealResourceWithDisplayName('json-value.js');
    await AxeCoreTestRunner.runValidation(UI.panels.resources.visibleView.contentElement);
  }

  async function testImageView() {
    await TestRunner.loadHTML(`
        <img src="../../resources/image.png">
      `);
    await ApplicationTestRunner.revealResourceWithDisplayName('image.png');
    await AxeCoreTestRunner.runValidation(UI.panels.resources.visibleView.contentElement);
  }

  TestRunner.runAsyncTestSuite(tests);
})();
