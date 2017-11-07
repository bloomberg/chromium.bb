// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Verify that navigator view removes mapped UISourceCodes.\n`);
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.loadModule('bindings_test_runner');
  await TestRunner.addScriptTag('resources/foo.js');

  var testMapping = BindingsTestRunner.initializeTestMapping();
  Runtime.experiments.enableForTest('persistence2');
  var sourcesNavigator = new Sources.SourcesNavigatorView();
  sourcesNavigator.show(UI.inspectorView.element);
  var fs = new BindingsTestRunner.TestFileSystem('file:///var/www');
  var fsEntry = BindingsTestRunner.addFooJSFile(fs);

  TestRunner.runTestSuite([
    function addFileSystem(next) {
      fs.reportCreated(next);
    },

    function waitForUISourceCodes(next) {
      var promises = [
        TestRunner.waitForUISourceCode('foo.js', Workspace.projectTypes.Network),
        TestRunner.waitForUISourceCode('foo.js', Workspace.projectTypes.FileSystem)
      ];
      Promise.all(promises).then(onUISourceCodesAdded);

      function onUISourceCodesAdded() {
        SourcesTestRunner.dumpNavigatorView(sourcesNavigator, true);
        next();
      }
    },

    function addFileMapping(next) {
      testMapping.addBinding('foo.js');
      BindingsTestRunner.waitForBinding('foo.js').then(onBindingCreated);

      function onBindingCreated(binding) {
        SourcesTestRunner.dumpNavigatorView(sourcesNavigator, true);
        next();
      }
    },

    function removeFileMapping(next) {
      Persistence.persistence.addEventListener(Persistence.Persistence.Events.BindingRemoved, onBindingRemoved);
      testMapping.removeBinding('foo.js');

      function onBindingRemoved(event) {
        var binding = event.data;
        if (binding.network.name() !== 'foo.js')
          return;
        Persistence.persistence.removeEventListener(Persistence.Persistence.Events.BindingRemoved, onBindingRemoved);
        SourcesTestRunner.dumpNavigatorView(sourcesNavigator, true);
        next();
      }
    },
  ]);
})();
