// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

(function(){

/**
 * Directory tree selector constants.
 */
const TREEITEM_A = TREEITEM_DRIVE + ' [entry-label="A"] ';
const TREEITEM_B = TREEITEM_A + '[entry-label="B"] ';
const TREEITEM_C = TREEITEM_B + '[entry-label="C"] ';

const TREEITEM_D = TREEITEM_DRIVE + ' [entry-label="D"] ';
const TREEITEM_E = TREEITEM_D + '[entry-label="E"] ';

/**
 * Entry set used for the folder shortcut tests.
 * @type {Array<TestEntryInfo>}
 */
const FOLDER_ENTRY_SET = [
  ENTRIES.directoryA,
  ENTRIES.directoryB,
  ENTRIES.directoryC,
  ENTRIES.directoryD,
  ENTRIES.directoryE,
  ENTRIES.directoryF
];

/**
 * Constants for each folder.
 * @type {Object}
 */
const DIRECTORY = {
  Drive: {
    contents: [
      ENTRIES.directoryA.getExpectedRow(), ENTRIES.directoryD.getExpectedRow()
    ],
    name: 'Drive',
    navItem: '.tree-item[entry-label="My Drive"]',
    treeItem: TREEITEM_DRIVE
  },
  A: {
    contents: [ENTRIES.directoryB.getExpectedRow()],
    name: 'A',
    navItem: '.tree-item[label="A"]',
    treeItem: TREEITEM_A
  },
  B: {
    contents: [ENTRIES.directoryC.getExpectedRow()],
    name: 'B',
    treeItem: TREEITEM_B
  },
  C: {
    contents: [],
    name: 'C',
    navItem: '.tree-item[label="C"]',
    treeItem: TREEITEM_C
  },
  D: {
    contents: [ENTRIES.directoryE.getExpectedRow()],
    name: 'D',
    navItem: '.tree-item[label="D"]',
    treeItem: TREEITEM_D
  },
  E: {
    contents: [ENTRIES.directoryF.getExpectedRow()],
    name: 'E',
    treeItem: TREEITEM_E
  }
};

/**
 * Expands tree item on the directory tree by clicking expand icon.
 * @param {string} windowId Target windowId.
 * @param {Object} directory Directory whose tree item should be expanded.
 * @return {Promise} Promise fulfilled on success.
 */
function expandTreeItem(windowId, directory) {
  const expand = directory.treeItem + '> .tree-row > .expand-icon';
  return remoteCall.waitForElement(windowId, expand).then(function() {
    return remoteCall.callRemoteTestUtil('fakeMouseClick', windowId, [expand]);
  }).then(function(result) {
    chrome.test.assertTrue(result);
    const expandedSubtree = directory.treeItem + '> .tree-children[expanded]';
    return remoteCall.waitForElement(windowId, expandedSubtree);
  });
}

/**
 * Expands whole directory tree.
 * @param {string} windowId Target windowId.
 * @return {Promise} Promise fulfilled on success.
 */
function expandDirectoryTree(windowId) {
  return expandTreeItem(windowId, DIRECTORY.Drive).then(function() {
    return expandTreeItem(windowId, DIRECTORY.A);
  }).then(function() {
    return expandTreeItem(windowId, DIRECTORY.B);
  }).then(function() {
    return expandTreeItem(windowId, DIRECTORY.D);
  });
}

/**
 * Makes |directory| the current directory.
 * @param {string} windowId Target windowId.
 * @param {Object} directory Directory which should be a current directory.
 * @return {Promise} Promise fulfilled on success.
 */
function navigateToDirectory(windowId, directory) {
  const icon = directory.treeItem + '> .tree-row > .item-icon';
  return remoteCall.waitForElement(windowId, icon).then(function() {
    return remoteCall.callRemoteTestUtil('fakeMouseClick', windowId, [icon]);
  }).then(function(result) {
    chrome.test.assertTrue(result);
    return remoteCall.waitForFiles(windowId, directory.contents);
  });
}

/**
 * Creates folder shortcut to |directory|.
 * The current directory must be a parent of the |directory|.
 * @param {string} windowId Target windowId.
 * @param {Object} directory Directory of shortcut to be created.
 * @return {Promise} Promise fulfilled on success.
 */
function createShortcut(windowId, directory) {
  return remoteCall.callRemoteTestUtil(
      'selectFile', windowId, [directory.name]).then(function(result) {
    chrome.test.assertTrue(result);
    return remoteCall.waitForElement(windowId, ['.table-row[selected]']);
  }).then(function() {
    return remoteCall.callRemoteTestUtil(
        'fakeMouseRightClick', windowId, ['.table-row[selected]']);
  }).then(function(result) {
    chrome.test.assertTrue(result);
    return remoteCall.waitForElement(
        windowId, '#file-context-menu:not([hidden])');
  }).then(function() {
    return remoteCall.waitForElement(
        windowId,
        '[command="#create-folder-shortcut"]:not([hidden]):not([disabled])');
  }).then(function() {
    return remoteCall.callRemoteTestUtil(
        'fakeMouseClick', windowId,
        ['[command="#create-folder-shortcut"]:not([hidden]):not([disabled])']);
  }).then(function(result) {
    chrome.test.assertTrue(result);
    return remoteCall.waitForElement(windowId, directory.navItem);
  });
}

/**
 * Removes folder shortcut to |directory|.
 * The current directory must be a parent of the |directory|.
 * @param {string} windowId Target windowId.
 * @param {Object} directory Directory of shortcut ot be removed.
 * @return {Promise} Promise fullfilled on success.
 */
function removeShortcut(windowId, directory) {
  // Focus the item first since actions are calculated asynchronously. The
  // context menu wouldn't show if there are no visible items. Focusing first,
  // will force the actions controller to refresh actions.
  // TODO(mtomasz): Remove this hack (if possible).
  return remoteCall.callRemoteTestUtil('focus',
      windowId, [directory.navItem]).then(function(result) {
    chrome.test.assertTrue(result);
    return remoteCall.callRemoteTestUtil(
      'fakeMouseRightClick',
      windowId,
      [directory.navItem]);
  }).then(function(result) {
    chrome.test.assertTrue(result);
    return remoteCall.waitForElement(
        windowId, '#roots-context-menu:not([hidden])');
  }).then(function() {
    return remoteCall.waitForElement(
        windowId,
        '[command="#remove-folder-shortcut"]:not([hidden]):not([disabled])');
  }).then(function() {
    return remoteCall.callRemoteTestUtil(
        'fakeMouseClick', windowId,
        ['#roots-context-menu [command="#remove-folder-shortcut"]:' +
         'not([hidden])']);
  }).then(function(result) {
    chrome.test.assertTrue(result);
    return remoteCall.waitForElementLost(windowId, directory.navItem);
  });
}

/**
 * Waits until the current directory become |currentDir| and folder shortcut to
 * |shortcutDir| is selected.
 * @param {string} windowId Target windowId.
 * @param {Object} currentDir Directory which should be a current directory.
 * @param {Object} shortcutDir Directory whose shortcut should be selected.
 * @return {Promise} Promise fullfilled on success.
 */
function expectSelection(windowId, currentDir, shortcutDir) {
  return remoteCall.waitForFiles(windowId, currentDir.contents).
      then(function() {
        return remoteCall.waitForElement(
            windowId, shortcutDir.navItem + '[selected]');
      });
}

/**
 * Clicks folder shortcut to |directory|.
 * @param {string} windowId Target windowId.
 * @param {Object} directory Directory whose shortcut will be clicked.
 * @return {Promise} Promise fullfilled with result of fakeMouseClick.
 */
function clickShortcut(windowId, directory) {
  return remoteCall.waitForElement(windowId, directory.navItem).
    then(function() {
      return remoteCall.callRemoteTestUtil(
          'fakeMouseClick', windowId, [directory.navItem]);
    });
}

/**
 * Creates some shortcuts and traverse them and some other directories.
 */
testcase.traverseFolderShortcuts = function() {
  let windowId;

  StepsRunner.run([
    // Open Files app on Drive.
    function() {
      setupAndWaitUntilReady(
          null, RootPath.DRIVE, this.next, [], FOLDER_ENTRY_SET);
    },
    // Expand the directory tree.
    function(results) {
      windowId = results.windowId;
      expandDirectoryTree(windowId).then(this.next);
    },
    // Create a shortcut to directory D.
    function() {
      createShortcut(windowId, DIRECTORY.D).then(this.next);
    },
    // Navigate to directory B.
    function() {
      navigateToDirectory(windowId, DIRECTORY.B).then(this.next);
    },
    // Create a shortcut to directory C.
    function() {
      createShortcut(windowId, DIRECTORY.C).then(this.next);
    },
    // Click the Drive root (My Drive) shortcut.
    function() {
      clickShortcut(windowId, DIRECTORY.Drive).then(this.next);
    },
    // Check: current directory and selection should be the Drive root.
    function() {
      expectSelection(
          windowId, DIRECTORY.Drive, DIRECTORY.Drive).then(this.next);
    },
    // Send Ctrl+3 key to file-list to select 3rd shortcut.
    function() {
      const key = ['#file-list', '3', '3', true, false, false];
      remoteCall.callRemoteTestUtil('fakeKeyDown', windowId, key, this.next);
    },
    // Check: current directory and selection should be D.
    function(result) {
      chrome.test.assertTrue(result);
      expectSelection(windowId, DIRECTORY.D, DIRECTORY.D).then(this.next);
    },
    // Send UpArrow key to directory tree to select the shortcut above D.
    function() {
      const key = ['#directory-tree', 'ArrowUp', 'Up', false, false, false];
      remoteCall.callRemoteTestUtil('fakeKeyDown', windowId, key, this.next);
    },
    // Check: current directory should be D, with shortcut C selected.
    function(result) {
      chrome.test.assertTrue(result);
      expectSelection(windowId, DIRECTORY.D, DIRECTORY.C).then(this.next);
    },
    // Send Enter key to the directory tree to change to directory C.
    function() {
      const key = ['#directory-tree', 'Enter', 'Enter', false, false, false];
      remoteCall.callRemoteTestUtil('fakeKeyDown', windowId, key, this.next);
    },
    // Check: current directory and selection should be C.
    function(result) {
      chrome.test.assertTrue(result);
      expectSelection(windowId, DIRECTORY.C, DIRECTORY.C).then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};

/**
 * Adds and removes shortcuts from other window and check if the active
 * directories and selected navigation items are correct.
 */
testcase.addRemoveFolderShortcuts = function() {
  let windowId1;
  let windowId2;

  function openFilesAppOnDrive() {
    let windowId;
    return new Promise(function(resolve) {
      return openNewWindow(null, RootPath.DRIVE, resolve);
    }).then(function(newWindowId) {
      windowId = newWindowId;
      return remoteCall.waitForElement(windowId, '#file-list');
    }).then(function() {
      return remoteCall.waitForFiles(windowId, DIRECTORY.Drive.contents);
    }).then(function() {
      return windowId;
    });
  }

  StepsRunner.run([
    // Add entries to Drive.
    function() {
      addEntries(['drive'], FOLDER_ENTRY_SET, this.next);
    },
    // Open one Files app window on Drive.
    function(result) {
      chrome.test.assertTrue(result);
      openFilesAppOnDrive().then(this.next);
    },
    // Open another Files app window on Drive.
    function(windowId) {
      windowId1 = windowId;
      openFilesAppOnDrive().then(this.next);
    },
    // Create a shortcut to D.
    function(windowId) {
      windowId2 = windowId;
      createShortcut(windowId1, DIRECTORY.D).then(this.next);
    },
    // Click the shortcut to D.
    function() {
      clickShortcut(windowId1, DIRECTORY.D).then(this.next);
    },
    // Check: current directory and selection should be D.
    function() {
      expectSelection(windowId1, DIRECTORY.D, DIRECTORY.D).then(this.next);
    },
    // Create a shortcut to A from the other window.
    function() {
      createShortcut(windowId2, DIRECTORY.A).then(this.next);
    },
    // Check: current directory and selection should still be D.
    function() {
      expectSelection(windowId1, DIRECTORY.D, DIRECTORY.D).then(this.next);
    },
    // Remove shortcut to D from the other window.
    function() {
      removeShortcut(windowId2, DIRECTORY.D).then(this.next);
    },
    // Check: directory D in the directory tree should be selected.
    function() {
      const selection = TREEITEM_D + '[selected]';
      remoteCall.waitForElement(windowId1, selection).then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};

})();
