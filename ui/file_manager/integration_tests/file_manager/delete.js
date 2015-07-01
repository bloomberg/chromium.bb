// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Delete menu item should be disabled when no item is selected.
testcase.deleteMenuItemIsDisabledWhenNoItemIsSelected = function() {
  testPromise(setupAndWaitUntilReady(null, RootPath.DOWNALOD).then(
      function(windowId) {
        // Right click the list without selecting an item.
        return remoteCall.callRemoteTestUtil(
            'fakeMouseRightClick', windowId, ['list.list']
            ).then(function(result) {
          chrome.test.assertTrue(result);

          // Wait until the context menu is shown.
          return remoteCall.waitForElement(
              windowId,
              '#file-context-menu:not([hidden])');
        }).then(function() {
          // Assert that delete command is disabled.
          return remoteCall.waitForElement(
              windowId,
              'cr-menu-item[command="#delete"][disabled="disabled"]');
        });
      }));
};
