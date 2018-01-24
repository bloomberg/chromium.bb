// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Verify that dirty uiSourceCodes are not bound.\n`);
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.loadModule('console_test_runner');
  await TestRunner.loadModule('bindings_test_runner');
  await TestRunner.addScriptTag('resources/foo.js');
  BindingsTestRunner.overrideNetworkModificationTime(
      {'http://127.0.0.1:8000/devtools/persistence/resources/foo.js': null});

  var networkUISourceCode = await TestRunner.waitForUISourceCode('foo.js', Workspace.projectTypes.Network);
  var content = await networkUISourceCode.requestContent();
  content = content.replace(/foo/g, 'bar');
  networkUISourceCode.setWorkingCopy(content);

  var fs = new BindingsTestRunner.TestFileSystem('file:///var/www');
  BindingsTestRunner.addFooJSFile(fs);
  fs.reportCreated(function() {});
  var binding = await TestRunner.addSnifferPromise(Persistence.Automapping.prototype, '_prevalidationFailedForTest');
  TestRunner.addResult('Failed to create binding: ' + binding);
  TestRunner.completeTest();
})();
