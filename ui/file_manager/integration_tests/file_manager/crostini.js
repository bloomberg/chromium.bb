// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

testcase.mountCrostiniContainer = function() {
  const fake = '#directory-tree .tree-item [root-type-icon="crostini"]';
  const real = '#directory-tree .tree-item [volume-type-icon="crostini"]';
  let appId;

  StepsRunner.run([
    function() {
      setupAndWaitUntilReady(
          null, RootPath.DOWNLOADS, this.next, [ENTRIES.hello], []);
    },
    function(results) {
      // Add entries to crostini volume, but do not mount.
      appId = results.windowId;
      addEntries(['crostini'], BASIC_CROSTINI_ENTRY_SET, this.next);
    },
    function() {
      // Linux Files fake root is shown.
      remoteCall.waitForElement(appId, fake).then(this.next);
    },
    function() {
      // Mount crostini, and ensure real root and files are shown.
      remoteCall.callRemoteTestUtil('fakeMouseClick', appId, [fake]);
      remoteCall.waitForElement(appId, real).then(this.next);
    },
    function() {
      const files = TestEntryInfo.getExpectedRows(BASIC_CROSTINI_ENTRY_SET);
      remoteCall.waitForFiles(appId, files).then(this.next);
    },
    function() {
      // Unmount and ensure fake root is shown.
      chrome.test.sendMessage(JSON.stringify({name: 'unmountCrostini'}));
      remoteCall.waitForElement(appId, fake).then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    },
  ]);
};
