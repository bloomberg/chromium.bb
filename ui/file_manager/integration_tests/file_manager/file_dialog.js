// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Tests to open and cancel the file dialog.
 *
 * @param {string} volumeName Volume name passed to the selectVolume remote
 *     funciton.
 * @param {Array<TestEntryInfo>} expectedSet Expected set of the entries.
 * @return {Promsie} Promise to be fulfilled/rejected depending on the test
 *     result.
 */
function openFileDialog(volumeName, expectedSet) {
  var localEntriesPromise = addEntries(['local'], BASIC_LOCAL_ENTRY_SET);
  var driveEntriesPromise = addEntries(['drive'], BASIC_DRIVE_ENTRY_SET);
  var setupPromise = Promise.all([localEntriesPromise, driveEntriesPromise]);

  var closeByCancelButtonPromise = setupPromise.then(function() {
    return openAndWaitForClosingDialog(
        {type: 'openFile'},
        volumeName,
        expectedSet,
        function(windowId) {
          return remoteCall.callRemoteTestUtil(
              'selectFile', windowId, ['hello.txt']
          ).then(function() {
            return remoteCall.waitForElement(windowId,
                                             '.button-panel button.cancel');
          }).then(function() {
            return remoteCall.callRemoteTestUtil(
                'fakeEvent',
                windowId,
                ['.button-panel button.cancel', 'click']);
          });
        });
  }).then(function(result) {
    // Undefined means the dialog is canceled.
    chrome.test.assertEq(undefined, result);
  });

  var closeByEscKeyPromise = closeByCancelButtonPromise.then(function() {
    return openAndWaitForClosingDialog(
        {type: 'openFile'},
        volumeName,
        expectedSet,
        function(windowId) {
          return remoteCall.callRemoteTestUtil(
              'selectFile', windowId, ['hello.txt']
          ).then(function() {
            return remoteCall.callRemoteTestUtil(
                'fakeKeyDown',
                windowId,
                ['#file-list', 'U+001B', false]);
          });
        });
  }).then(function(result) {
    // Undefined means the dialog is canceled.
    chrome.test.assertEq(undefined, result);
  });

  var closeByOkButtonPromise = closeByEscKeyPromise.then(function() {
    return openAndWaitForClosingDialog(
        {type: 'openFile'},
        volumeName,
        expectedSet,
        function(windowId) {
          return remoteCall.callRemoteTestUtil(
              'selectFile', windowId, ['hello.txt']
          ).then(function() {
            return remoteCall.waitForElement(windowId,
                                             '.button-panel button.ok');
          }).then(function() {
            return remoteCall.callRemoteTestUtil(
                'fakeEvent',
                windowId,
                ['.button-panel button.ok', 'click']);
          });
        });
  }).then(function(result) {
    chrome.test.assertEq('hello.txt', result.name);
  });

  return closeByOkButtonPromise;
}

testcase.openFileDialogOnDownloads = function() {
  testPromise(openFileDialog('downloads', BASIC_LOCAL_ENTRY_SET));
};

testcase.openFileDialogOnDrive = function() {
  testPromise(openFileDialog('drive', BASIC_DRIVE_ENTRY_SET));
};

testcase.unloadFileDialog = function() {
  chrome.fileSystem.chooseEntry({type: 'openFile'}, function(entry) {});

  testPromise(remoteCall.waitForWindow('dialog#').then(function(windowId) {
    return remoteCall.callRemoteTestUtil('unload', windowId, []).
        then(function() {
          return remoteCall.callRemoteTestUtil('getErrorCount', windowId, []);
        }).
        then(function(num) {
          chrome.test.assertEq(0, num);
        });
  }));
};
