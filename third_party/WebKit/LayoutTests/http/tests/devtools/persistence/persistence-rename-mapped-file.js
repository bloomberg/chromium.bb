// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Verify that binding gets removed as the fileSystem file gets renamed.\n`);
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.loadModule('bindings_test_runner');
  await TestRunner.addScriptTag('resources/foo.js');

  BindingsTestRunner.forceUseDefaultMapping();
  var fs = new BindingsTestRunner.TestFileSystem('file:///var/www');
  var fsEntry = BindingsTestRunner.addFooJSFile(fs);
  fs.addFileMapping('http://127.0.0.1:8000', '/');
  fs.reportCreated(function() {});
  BindingsTestRunner.waitForBinding('foo.js').then(onBindingCreated);
  Persistence.persistence.addEventListener(Persistence.Persistence.Events.BindingRemoved, onBindingRemoved);

  function onBindingCreated(binding) {
    TestRunner.addResult('Binding created: ' + binding);
    TestRunner.addResult('Renaming foo.js -> bar.js');
    binding.fileSystem.rename('bar.js', function() {});
  }

  function onBindingRemoved(event) {
    var binding = event.data;
    TestRunner.addResult('Binding successfully removed: ' + binding);
    TestRunner.completeTest();
  }
})();
