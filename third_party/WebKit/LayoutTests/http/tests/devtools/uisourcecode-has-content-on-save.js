// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Test to ensure on save the content has been loaded in the UISourceCode.\n`);

  await TestRunner.loadHTML(`
    <!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/html4/loose.dtd">
    <div id="output"></div>
  `);
  await TestRunner.addScriptTag('../../resources/dummy.js');

  Workspace.fileManager.save = function(url, content) {
    TestRunner.addResult('Saving');
    TestRunner.addResult('URL: ' + url);
    TestRunner.addResult('Data: ' + content);
    TestRunner.completeTest();
    return Promise.resolve(false);
  };

  var uiSourceCode = Workspace.workspace.uiSourceCodeForURL('http://127.0.0.1:8000/resources/dummy.js');
  uiSourceCode.saveAs();
})();
