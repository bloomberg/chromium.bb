// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function () {
    TestRunner.addResult(`Tests accessibility in the preview view for XML files using the axe-core linter.\n`);
    await TestRunner.loadModule('application_test_runner');
    await TestRunner.loadModule('axe_core_test_runner');

    // Note: every test that uses a storage API must manually clean-up state from previous tests.
    await ApplicationTestRunner.resetState();
    await TestRunner.loadModule('console_test_runner');
    await TestRunner.showPanel('resources');

    ApplicationTestRunner.dumpCurrentState('Initial state:');

    async function testXMLView() {
      await TestRunner.addHTMLImport('../../network/resources/xml-with-stylesheet.xml');
      await ApplicationTestRunner.revealResourceWithDisplayName('xml-with-stylesheet.xml');
      await AxeCoreTestRunner.runValidation(UI.panels.resources.visibleView.contentElement);
    }

    TestRunner.runAsyncTestSuite([testXMLView]);
  })();
