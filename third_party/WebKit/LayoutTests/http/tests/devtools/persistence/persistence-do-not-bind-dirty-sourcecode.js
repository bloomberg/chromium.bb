// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Verify that dirty uiSourceCodes are not bound.\n`);
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.loadModule('console_test_runner');
  await TestRunner.loadModule('bindings_test_runner');
  await TestRunner.addScriptTag('resources/foo.js');

  Runtime.experiments.enableForTest('persistence2');
  var testMapping = BindingsTestRunner.initializeTestMapping();
  var fs = new BindingsTestRunner.TestFileSystem('file:///var/www');
  BindingsTestRunner.addFooJSFile(fs);
  fs.reportCreated(function() {});

  TestRunner.runTestSuite([
    function waitForUISourceCodes(next) {
      Promise
          .all([
            TestRunner.waitForUISourceCode('foo.js', Workspace.projectTypes.Network)
                .then(uiSourceCode => uiSourceCode.setWorkingCopy('dirty.')),
            TestRunner.waitForUISourceCode('foo.js', Workspace.projectTypes.FileSystem)
          ])
          .then(next);
    },

    function addFileSystemMapping(next) {
      TestRunner.addSniffer(Persistence.Persistence.prototype, '_prevalidationFailedForTest', onPrevalidationFailed);
      testMapping.addBinding('foo.js');

      function onPrevalidationFailed(binding) {
        TestRunner.addResult('Failed to create binding: ' + binding);
        next();
      }
    },
  ]);
})();
