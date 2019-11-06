// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests resource tree model on crafted iframe addition (will time out on failure).\n`);
  await TestRunner.loadModule('application_test_runner');
  await TestRunner.showPanel('resources');
  TestRunner.addSniffer(TestRunner.resourceTreeModel, '_frameAttached', TestRunner.completeTest);
  TestRunner.evaluateInPage(`
    (function createCraftedIframe() {
      var fabricatedFrame = document.createElement("iframe");
      fabricatedFrame.src = "#foo";
      document.body.appendChild(fabricatedFrame);
      fabricatedFrame.contentDocument.write("<div>bar</div>");
    })();
  `);
})();
