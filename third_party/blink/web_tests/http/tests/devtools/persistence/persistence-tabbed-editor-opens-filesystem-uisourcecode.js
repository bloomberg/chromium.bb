// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  'use strict';
  TestRunner.addResult(
      `Verify that for a fileSystem UISourceCode with persistence binding TabbedEditorContainer opens filesystem UISourceCode.\n`);
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.loadModule('bindings_test_runner');
  await TestRunner.addScriptTag('resources/foo.js');
  await TestRunner.showPanel('sources');

  var testMapping = BindingsTestRunner.initializeTestMapping();
  var fs = new BindingsTestRunner.TestFileSystem('file:///var/www');
  var fsEntry = BindingsTestRunner.addFooJSFile(fs);
  fs.reportCreated(function() {});
  testMapping.addBinding('foo.js');
  BindingsTestRunner.waitForBinding('foo.js').then(onBindingCreated);

  function onBindingCreated(binding) {
    TestRunner.addResult('Binding created: ' + binding);
    dumpEditorTabs('Opened tabs before opening any UISourceCodes:');
    TestRunner.addResult('request open uiSourceCode: ' + binding.fileSystem.url());
    UI.panels.sources.showUISourceCode(binding.network, 0, 0);
    dumpEditorTabs('Opened tabs after opening UISourceCode:');
    TestRunner.completeTest();
  }

  function dumpEditorTabs(title) {
    var editorContainer = UI.panels.sources._sourcesView._editorContainer;
    var openedUISourceCodes = [...editorContainer._tabIds.keys()];
    openedUISourceCodes.sort((a, b) => a.url().compareTo(b.url()));
    TestRunner.addResult(title);
    for (const code of openedUISourceCodes)
      TestRunner.addResult('    ' + code.url());
  }
})();
