// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Verify that GoTo source dialog filters out mapped uiSourceCodes.\n`);
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.loadModule('bindings_test_runner');
  await TestRunner.addScriptTag('resources/foo.js');

  var testMapping = BindingsTestRunner.initializeTestMapping();
  var fs = new BindingsTestRunner.TestFileSystem('file:///var/www');
  BindingsTestRunner.addFooJSFile(fs);
  fs.reportCreated(function() {});

  TestRunner.runTestSuite([
    function waitForUISourceCodes(next) {
      Promise
          .all([
            TestRunner.waitForUISourceCode('foo.js', Workspace.projectTypes.Network),
            TestRunner.waitForUISourceCode('foo.js', Workspace.projectTypes.FileSystem)
          ])
          .then(next);
    },

    function goToSourceDialogBeforeBinding(next) {
      dumpGoToSourceDialog(next);
    },

    function addFileSystemMapping(next) {
      testMapping.addBinding('foo.js');
      BindingsTestRunner.waitForBinding('foo.js').then(next);
    },

    function goToSourceAfterBinding(next) {
      dumpGoToSourceDialog(next);
    },
  ]);

  function dumpGoToSourceDialog(next) {
    QuickOpen.QuickOpen.show('');
    TestRunner.addSnifferPromise(QuickOpen.QuickOpen.prototype, '_providerLoadedForTest').then(provider => {
      var keys = [];
      for (var i = 0; i < provider.itemCount(); ++i)
        keys.push(provider.itemKeyAt(i));
      keys.sort();
      TestRunner.addResult(keys.join('\n'));
      UI.Dialog._instance.hide();
      next();
    });
  }
})();
