// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Expected autocomplete results for 'hello'.
 * @type {Array<string>}
 * @const
 */
var EXPECTED_AUTOCOMPLETE_LIST = [
  '\'hello\' - search Drive',
  'hello.txt'
];

/**
 * Expected files shown in the search results for 'hello'
 *
 * @type {Array<TestEntryInfo>}
 * @const
 */
var SEARCH_RESULTS_ENTRY_SET = [
  ENTRIES.hello
];

/**
 * Returns the steps to start a search for 'hello' and wait for the
 * autocomplete results to appear.
 */
function getStepsForSearchResultsAutoComplete() {
  var appId;
  var steps = [
    function() {
      setupAndWaitUntilReady(null, RootPath.DRIVE, this.next);
    },
    // Focus the search box.
    function(results) {
      appId = results.windowId;
      remoteCall.callRemoteTestUtil(
          'fakeEvent', appId, ['#search-box cr-input', 'focus'], this.next);
    },
    // Input a text.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.callRemoteTestUtil(
          'inputText', appId, ['#search-box cr-input', 'hello'], this.next);
    },
    // Notify the element of the input.
    function() {
      remoteCall.callRemoteTestUtil(
          'fakeEvent', appId, ['#search-box cr-input', 'input'], this.next);
    },
    // Wait for the auto complete list getting the expected contents.
    function(result) {
      chrome.test.assertTrue(result);
      var caller = getCaller();
      repeatUntil(function() {
        return remoteCall.callRemoteTestUtil('queryAllElements',
                                             appId,
                                             ['#autocomplete-list li']).
            then(function(elements) {
              var list = elements.map(
                  function(element) { return element.text; });
              return chrome.test.checkDeepEq(EXPECTED_AUTOCOMPLETE_LIST, list) ?
                  undefined :
                  pending(caller, 'Current auto complete list: %j.', list);
            });
        }).
        then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    },
    function() {
      this.next(appId);
    }
  ];
  return steps;
}

/**
 * Tests opening the "Offline" on the sidebar navigation by clicking the icon,
 * and checks contents of the file list. Only the entries "available offline"
 * should be shown. "Available offline" entries are hosted documents and the
 * entries cached by DriveCache.
 */
testcase.driveOpenSidebarOffline = function() {
  var appId;
  StepsRunner.run([
    function() {
      setupAndWaitUntilReady(null, RootPath.DRIVE, this.next);
    },
    // Click the icon of the Offline volume.
    function(results) {
      appId = results.windowId;
      remoteCall.callRemoteTestUtil(
        'selectVolume', appId, ['drive_offline'], this.next);
    },
    // Wait until the file list is updated.
    function(result) {
      chrome.test.assertFalse(!result);
      remoteCall.waitForFileListChange(appId, BASIC_DRIVE_ENTRY_SET.length).
          then(this.next);
    },
    // Verify the file list.
    function(actualFilesAfter) {
      chrome.test.assertEq(
          TestEntryInfo.getExpectedRows(OFFLINE_ENTRY_SET).sort(),
          actualFilesAfter);
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};

/**
 * Tests opening the "Shared with me" on the sidebar navigation by clicking the
 * icon, and checks contents of the file list. Only the entries labeled with
 * "shared-with-me" should be shown.
 */
testcase.driveOpenSidebarSharedWithMe = function() {
  var appId;
  StepsRunner.run([
    function() {
      setupAndWaitUntilReady(null, RootPath.DRIVE, this.next);
    },
    // Click the icon of the Shared With Me volume.
    function(results) {
      appId = results.windowId;
      // Use the icon for a click target.
      remoteCall.callRemoteTestUtil('selectVolume',
                                    appId,
                                    ['drive_shared_with_me'], this.next);
    },
    // Wait until the file list is updated.
    function(result) {
      chrome.test.assertFalse(!result);
      remoteCall.waitForFileListChange(appId, BASIC_DRIVE_ENTRY_SET.length).
          then(this.next);
    },
    // Verify the file list.
    function(actualFilesAfter) {
      chrome.test.assertEq(
          TestEntryInfo.getExpectedRows(SHARED_WITH_ME_ENTRY_SET).sort(),
          actualFilesAfter);
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};

/**
 * Tests autocomplete with a query 'hello'.
 */
testcase.driveAutoCompleteQuery = function() {
  StepsRunner.run(getStepsForSearchResultsAutoComplete());
};

/**
 * Tests that clicking the first option in the autocomplete box shows all of
 * the results for that query.
 */
testcase.driveClickFirstSearchResult = function() {
  var appId;
  var steps = getStepsForSearchResultsAutoComplete();
  steps.push(
    function(id) {
      appId = id;
      remoteCall.callRemoteTestUtil(
          'fakeKeyDown', appId,
          ['#autocomplete-list', 'ArrowDown', 'Down', false, false, false],
          this.next);
    },
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.waitForElement(
          appId,
          ['#autocomplete-list li[selected]']).
          then(this.next);
    },
    function(result) {
      remoteCall.callRemoteTestUtil(
          'fakeMouseDown', appId,
          ['#autocomplete-list li[selected]'],
          this.next);
    },
    function(result)
    {
      remoteCall.waitForFileListChange(appId, BASIC_DRIVE_ENTRY_SET.length).
      then(this.next);
    },
    function(actualFilesAfter)
    {
      chrome.test.assertEq(
          TestEntryInfo.getExpectedRows(SEARCH_RESULTS_ENTRY_SET).sort(),
          actualFilesAfter);
      checkIfNoErrorsOccured(this.next);
    }
  );

  StepsRunner.run(steps);
};

/**
 * Tests that pressing enter after typing a search shows all of
 * the results for that query.
 */
testcase.drivePressEnterToSearch = function() {
  var appId;
  var steps = getStepsForSearchResultsAutoComplete();
  steps.push(
      function(id) {
        appId = id;
        remoteCall.callRemoteTestUtil(
            'fakeEvent', appId, ['#search-box cr-input', 'focus'], this.next);
      },
      function(result) {
        remoteCall.callRemoteTestUtil(
            'fakeKeyDown', appId,
            ['#search-box cr-input', 'Enter', 'Enter', false, false, false],
            this.next);
      },
      function(result) {
        remoteCall.waitForFileListChange(appId, BASIC_DRIVE_ENTRY_SET.length)
            .then(this.next);
      },
      function(actualFilesAfter) {
        chrome.test.assertEq(
            TestEntryInfo.getExpectedRows(SEARCH_RESULTS_ENTRY_SET).sort(),
            actualFilesAfter);
        checkIfNoErrorsOccured(this.next);
      });

  StepsRunner.run(steps);
};

/**
 * Tests pinning a file to a mobile network.
 */
testcase.drivePinFileMobileNetwork = function() {
  testPromise(setupAndWaitUntilReady(null, RootPath.DRIVE).then(
      function(results) {
        var windowId = results.windowId;
        var caller = getCaller();
        return sendTestMessage(
            {name: 'useCellularNetwork'}).then(function(result) {
          return remoteCall.callRemoteTestUtil(
              'selectFile', windowId, ['hello.txt']);
        }).then(function() {
          return repeatUntil(function() {
            return navigator.connection.type != 'cellular' ?
                pending(caller, 'Network state is not changed to cellular.') :
                null;
          });
        }).then(function() {
          return remoteCall.waitForElement(windowId, ['.table-row[selected]']);
        }).then(function() {
          return remoteCall.callRemoteTestUtil(
              'fakeMouseRightClick', windowId, ['.table-row[selected]']);
        }).then(function(result) {
          chrome.test.assertTrue(result);
          return remoteCall.waitForElement(
              windowId, '#file-context-menu:not([hidden])');
        }).then(function() {
          return remoteCall.waitForElement(windowId,
              ['[command="#toggle-pinned"]']);
        }).then(function() {
          return remoteCall.callRemoteTestUtil(
              'fakeMouseClick', windowId, ['[command="#toggle-pinned"]']);
        }).then(function(result) {
          return remoteCall.waitForElement(
              windowId, '#file-context-menu[hidden]');
        }).then(function() {
          return remoteCall.callRemoteTestUtil(
              'fakeEvent', windowId, ['#file-list', 'contextmenu']);
        }).then(function(result) {
          chrome.test.assertTrue(result);
          return remoteCall.waitForElement(
              windowId, '[command="#toggle-pinned"][checked]');
        }).then(function() {
          return repeatUntil(function() {
            return remoteCall.callRemoteTestUtil(
                'getNotificationIDs', null, []).then(function(idSet) {
              return !idSet['disabled-mobile-sync'] ?
                  pending(caller, 'Sync disable notification is not found.') :
                  null;
            });
          });
        }).then(function() {
          return sendTestMessage({
            name: 'clickNotificationButton',
            extensionId: FILE_MANAGER_EXTENSIONS_ID,
            notificationId: 'disabled-mobile-sync',
            index: 0
          });
        }).then(function() {
          return repeatUntil(function() {
            return remoteCall.callRemoteTestUtil(
                'getPreferences', null, []).then(function(preferences) {
              return preferences.cellularDisabled ?
                  pending(caller, 'Drive sync is still disabled.') : null;
            });
          });
        });
      }));
};

/**
 * Tests that pressing Ctrl+A (select all files) from the search box doesn't put
 * the Files App into check-select mode (crbug.com/849253).
 */
testcase.drivePressCtrlAFromSearch = function() {
  var appId;
  var steps = [
    function() {
      setupAndWaitUntilReady(null, RootPath.DRIVE, this.next);
    },
    // Focus the search box.
    function(results) {
      appId = results.windowId;
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, ['#search-button'], this.next);
    },
    // Wait for the search box to be visible.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.waitForElement(appId, ['#search-box cr-input:not([hidden])'])
          .then(this.next);
    },
    // Press Ctrl+A inside the search box.
    function(result) {
      remoteCall.callRemoteTestUtil(
          'fakeKeyDown', appId,
          ['#search-box cr-input', 'A', 'A', true, false, false], this.next);
    },
    // Check we didn't enter check-select mode.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.waitForElement(appId, ['body:not(.check-select)'])
          .then(this.next);
    },
    function(result) {
      checkIfNoErrorsOccured(this.next);
    },
  ];
  StepsRunner.run(steps);
};

testcase.PRE_driveMigratePinnedFile = function() {
  // Pin a file.
  testPromise(
      setupAndWaitUntilReady(null, RootPath.DRIVE).then(function(results) {
        var windowId = results.windowId;
        return remoteCall
            .callRemoteTestUtil('selectFile', windowId, ['hello.txt'])
            .then(function() {
              return remoteCall.waitForElement(
                  windowId, ['.table-row[selected]']);
            })
            .then(function() {
              return remoteCall.callRemoteTestUtil(
                  'fakeMouseRightClick', windowId, ['.table-row[selected]']);
            })
            .then(function(result) {
              chrome.test.assertTrue(result);
              return remoteCall.waitForElement(
                  windowId, '#file-context-menu:not([hidden])');
            })
            .then(function() {
              return remoteCall.waitForElement(
                  windowId, ['[command="#toggle-pinned"]']);
            })
            .then(function() {
              return remoteCall.callRemoteTestUtil(
                  'fakeMouseClick', windowId, ['[command="#toggle-pinned"]']);
            })
            .then(function(result) {
              return remoteCall.waitForElement(
                  windowId, '#file-context-menu[hidden]');
            })
            .then(function() {
              return remoteCall.callRemoteTestUtil(
                  'fakeEvent', windowId, ['#file-list', 'contextmenu']);
            })
            .then(function(result) {
              chrome.test.assertTrue(result);
              return remoteCall.waitForElement(
                  windowId, '[command="#toggle-pinned"][checked]');
            });
      }));
};

testcase.driveMigratePinnedFile = function() {
  // After enabling DriveFS, ensure the file is still pinned.
  testPromise(
      setupAndWaitUntilReady(null, RootPath.DRIVE).then(function(results) {
        var windowId = results.windowId;
        return remoteCall
            .callRemoteTestUtil('selectFile', windowId, ['hello.txt'])
            .then(function() {
              return remoteCall.waitForElement(
                  windowId, ['.table-row[selected]']);
            })
            .then(function() {
              return remoteCall.callRemoteTestUtil(
                  'fakeMouseRightClick', windowId, ['.table-row[selected]']);
            })
            .then(function(result) {
              chrome.test.assertTrue(result);
              return remoteCall.waitForElement(
                  windowId, '#file-context-menu:not([hidden])');
            })
            .then(function() {
              return remoteCall.waitForElement(
                  windowId, '[command="#toggle-pinned"][checked]');
            });
      }));

};

// Match the way the production version formats dates.
function formatDate(date) {
  var padAndConvert = function(i) {
    return (i < 10 ? '0' : '') + i.toString();
  };

  var year = date.getFullYear().toString();
  // Months are 0-based, but days aren't.
  var month = padAndConvert(date.getMonth() + 1);
  var day = padAndConvert(date.getDate());

  return `${year}-${month}-${day}`;
}

testcase.driveBackupPhotos = function() {
  let appId;

  const USB_VOLUME_QUERY = '#directory-tree [volume-type-icon="removable"]';
  let date;

  StepsRunner.run([
    // Open Files app on local downloads.
    function() {
      setupAndWaitUntilReady(null, RootPath.DOWNLOADS, this.next);
    },
    // Mount USB volume in the Downloads window.
    function(results) {
      appId = results.windowId;
      chrome.test.sendMessage(
          JSON.stringify({name: 'mountFakeUsbDcim'}), this.next);
    },
    // Wait for the USB mount.
    function() {
      remoteCall.waitForElement(appId, USB_VOLUME_QUERY).then(this.next);
    },
    // Navigate to the DCIM directory.
    function() {
      remoteCall
          .navigateWithDirectoryTree(appId, '/DCIM', 'fake-usb', 'removable')
          .then(this.next);
    },
    // Wait for the import button to be ready.
    function() {
      remoteCall
          .waitForElement(
              appId, '#cloud-import-button [icon="files:cloud-upload"]')
          .then(this.next);
    },
    // Start the import.
    function() {
      date = new Date();
      remoteCall
          .callRemoteTestUtil('fakeMouseClick', appId, ['#cloud-import-button'])
          .then(this.next);
    },
    // Wait for the image to be marked as imported.
    function(success) {
      chrome.test.assertTrue(success);
      remoteCall
          .waitForElement(appId, '.status-icon[file-status-icon="imported"]')
          .then(this.next);
    },
    // Navigate to today's backup directory in Drive.
    function() {
      const formattedDate = formatDate(date);
      remoteCall
          .navigateWithDirectoryTree(
              appId, `/root/Chrome OS Cloud backup/${formattedDate}`,
              'My Drive', 'drive')
          .then(this.next);
    },
    // Verify the backed-up file list contains only a copy of the image within
    // DCIM in the removable storage.
    function() {
      const files = TestEntryInfo.getExpectedRows([ENTRIES.image3]);
      remoteCall.waitForFiles(appId, files, {ignoreLastModifiedTime: true})
          .then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};
