// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const crostini = {};

crostini.testCrostiniNotEnabled = (done) => {
  chrome.fileManagerPrivate.crostiniEnabled_ = false;
  test.setupAndWaitUntilReady()
      .then(() => {
        fileManager.setupCrostini_();
        return test.waitForElementLost(
            '#directory-tree .tree-item [root-type-icon="crostini"]');
      })
      .then(() => {
        // Reset crostini back to default enabled=true.
        chrome.fileManagerPrivate.crostiniEnabled_ = true;
        done();
      });
};

crostini.testMountCrostiniSuccess = (done) => {
  const oldMount = chrome.fileManagerPrivate.mountCrostiniContainer;
  let mountCallback = null;
  chrome.fileManagerPrivate.mountCrostiniContainer = (callback) => {
    mountCallback = callback;
  };
  test.setupAndWaitUntilReady()
      .then(() => {
        // Linux files fake root is shown.
        return test.waitForElement(
            '#directory-tree .tree-item [root-type-icon="crostini"]');
      })
      .then(() => {
        // Click on Linux files.
        assertTrue(
            test.fakeMouseClick(
                '#directory-tree .tree-item [root-type-icon="crostini"]'),
            'click linux files');
        return test.waitForElement('paper-progress:not([hidden])');
      })
      .then(() => {
        // Ensure mountCrostiniContainer is called.
        return test.repeatUntil(() => {
          if (!mountCallback)
            return test.pending('Waiting for mountCrostiniContainer');
          return mountCallback;
        });
      })
      .then(() => {
        // Intercept the fileManagerPrivate.mountCrostiniContainer call
        // and add crostini disk mount.
        test.mountCrostini();
        // Continue from fileManagerPrivate.mountCrostiniContainer callback
        // and ensure expected files are shown.
        mountCallback();
        return test.waitForFiles(
            test.TestEntryInfo.getExpectedRows(test.BASIC_CROSTINI_ENTRY_SET));
      })
      .then(() => {
        // Reset fileManagerPrivate.mountCrostiniContainer and remove mount.
        chrome.fileManagerPrivate.mountCrostiniContainer = oldMount;
        chrome.fileManagerPrivate.removeMount('crostini');
        // Linux Files fake root is shown.
        return test.waitForElement(
            '#directory-tree .tree-item [root-type-icon="crostini"]');
      })
      .then(() => {
        // Downloads folder should be shown when crostini goes away.
        return test.waitForFiles(
            test.TestEntryInfo.getExpectedRows(test.BASIC_LOCAL_ENTRY_SET));
      })
      .then(() => {
        done();
      });
};

crostini.testMountCrostiniError = (done) => {
  const oldMount = chrome.fileManagerPrivate.mountCrostiniContainer;
  // Override fileManagerPrivate.mountCrostiniContainer to return error.
  chrome.fileManagerPrivate.mountCrostiniContainer = (callback) => {
    chrome.runtime.lastError = {message: 'test message'};
    callback();
    delete chrome.runtime.lastError;
  };
  test.setupAndWaitUntilReady()
      .then(() => {
        return test.waitForElement(
            '#directory-tree .tree-item [root-type-icon="crostini"]');
      })
      .then(() => {
        // Click on Linux Files, ensure error dialog is shown.
        assertTrue(test.fakeMouseClick(
            '#directory-tree .tree-item [root-type-icon="crostini"]'));
        return test.waitForElement('.cr-dialog-container.shown');
      })
      .then(() => {
        // Click OK button to close.
        assertTrue(test.fakeMouseClick('button.cr-dialog-ok'));
        return test.waitForElementLost('.cr-dialog-container.shown');
      })
      .then(() => {
        // Reset chrome.fileManagerPrivate.mountCrostiniContainer.
        chrome.fileManagerPrivate.mountCrostiniContainer = oldMount;
        done();
      });
};

crostini.testCrostiniMountOnDrag = (done) => {
  chrome.fileManagerPrivate.mountCrostiniContainerDelay_ = 0;
  test.setupAndWaitUntilReady()
      .then(() => {
        return test.waitForElement(
            '#directory-tree .tree-item [root-type-icon="crostini"]');
      })
      .then(() => {
        assertTrue(test.sendEvent(
            '#directory-tree .tree-item [root-type-icon="crostini"]',
            new Event('dragenter', {bubbles: true})));
        assertTrue(test.sendEvent(
            '#directory-tree .tree-item [root-type-icon="crostini"]',
            new Event('dragleave', {bubbles: true})));
        return test.waitForFiles(
            test.TestEntryInfo.getExpectedRows(test.BASIC_CROSTINI_ENTRY_SET));
      })
      .then(() => {
        chrome.fileManagerPrivate.removeMount('crostini');
        return test.waitForElement(
            '#directory-tree .tree-item [root-type-icon="crostini"]');
      })
      .then(() => {
        done();
      });
};

crostini.testErrorOpeningDownloadsWithCrostiniApp = (done) => {
  // Save old fmp.getFileTasks and replace with version that returns
  // crostini app and chrome Text app.
  let oldGetFileTasks = chrome.fileManagerPrivate.getFileTasks;
  chrome.fileManagerPrivate.getFileTasks = (entries, callback) => {
    setTimeout(callback, 0, [
      {
        taskId: 'text-app-id|app|text',
        title: 'Text',
        verb: 'open_with',
      },
      {
        taskId: 'crostini-app-id|crostini|open-with',
        title: 'Crostini App',
        verb: 'open_with',
      }
    ]);
  };

  test.setupAndWaitUntilReady()
      .then(() => {
        // Right click on 'hello.txt' file, wait for dialog with 'Open with'.
        assertTrue(
            test.fakeMouseRightClick('#listitem-' + test.maxListItemId()));
        return test.waitForElement(
            'cr-menu-item[command="#open-with"]:not([hidden]');
      })
      .then(() => {
        // Click 'Open with', wait for picker.
        assertTrue(test.fakeMouseClick('cr-menu-item[command="#open-with"]'));
        return test.waitForElement('#default-tasks-list');
      })
      .then(() => {
        // Ensure picker shows both options.  Click on 'Crostini App'.  Ensure
        // error is shown.
        const list = document.querySelectorAll('#default-tasks-list li div');
        assertEquals(2, list.length);
        assertEquals('Open with Crostini App', list[0].innerText);
        assertEquals('Open with Text', list[1].innerText);
        assertTrue(test.fakeMouseClick('#default-tasks-list li'));
        return test.repeatUntil(() => {
          return document.querySelector('.cr-dialog-title').innerText ===
              'Unable to open with Crostini App' ||
              test.pending('Waiting for Unable to open dialog');
        });
      })
      .then(() => {
        // Validate error messages, click 'OK' to close.  Ensure dialog closes.
        assertEquals(
            'To open files with Crostini App, ' +
                'first copy to Linux files folder.',
            document.querySelector('.cr-dialog-text').innerText);
        assertTrue(test.fakeMouseClick('button.cr-dialog-ok'));
        return test.waitForElementLost('.cr-dialog-container.shown');
      })
      .then(() => {
        // Restore fmp.getFileTasks.
        chrome.fileManagerPrivate.getFileTasks = oldGetFileTasks;
        done();
      });
};

crostini.testSharePathCrostiniSuccess = (done) => {
  let sharePathCalled = false;
  chrome.fileManagerPrivate.sharePathWithCrostiniContainer = (callback) => {
    sharePathCalled = true;
  };
  test.setupAndWaitUntilReady()
      .then(() => {
        // Right-click 'photos' directory.
        // Check 'Share with Linux' is shown in menu.
        test.selectFile('photos');
        assertTrue(
            test.fakeMouseRightClick('#file-list li[selected]'),
            'right-click photos');
        return test.waitForElement(
            '#file-context-menu:not([hidden]) ' +
            '[command="#share-with-linux"]:not([hidden]):not([disabled])');
      })
      .then(() => {
        // Click on 'Start with Linux'.
        assertTrue(
            test.fakeMouseClick(
                '#file-context-menu [command="#share-with-linux"]'),
            'Share with Linux');
        return test.waitForElement('#file-context-menu[hidden]');
      })
      .then(() => {
        // Check sharePathWithCrostiniContainer is called.
        assertTrue(sharePathCalled);
        done();
      });
};

// Verify right-click menu with 'Share with Linux' is not shown for:
// * Root Downloads folder
// * Any folder outside of downloads (e.g. crostini or drive)
crostini.testSharePathNotShown = (done) => {
  const myFiles = '#directory-tree .tree-item [root-type-icon="my_files"]';
  const downloads = '#file-list li [file-type-icon="downloads"]';
  const linuxFiles = '#directory-tree .tree-item [root-type-icon="crostini"]';
  const googleDrive = '#directory-tree .tree-item [volume-type-icon="drive"]';
  const menuNoShareWithLinux = '#file-context-menu:not([hidden]) ' +
      '[command="#share-with-linux"][hidden][disabled="disabled"]';

  test.setupAndWaitUntilReady()
      .then(() => {
        // Select 'My files' in directory tree to show Downloads in file list.
        assertTrue(test.fakeMouseClick(myFiles), 'click My files');
        return test.waitForElement(downloads);
      })
      .then(() => {
        // Right-click 'Downloads' directory.
        // Check 'Share with Linux' is not shown in menu.
        assertTrue(
            test.fakeMouseRightClick(downloads), 'right-click downloads');
        return test.waitForElement(menuNoShareWithLinux);
      })
      .then(() => {
        // Select 'Linux files' in directory tree to show dir A in file list.
        return test.waitForElement(linuxFiles);
      })
      .then(() => {
        assertTrue(test.fakeMouseClick(linuxFiles), 'click Linux files');
        return test.waitForFiles(
            test.TestEntryInfo.getExpectedRows(test.BASIC_CROSTINI_ENTRY_SET));
      })
      .then(() => {
        // Check 'Share with Linux' is not shown in menu.
        test.selectFile('A');
        assertTrue(
            test.fakeMouseRightClick('#file-list li[selected]'),
            'right-click directory A');
        return test.waitForElement(menuNoShareWithLinux);
      })
      .then(() => {
        // Select 'Google Drive' to show dir photos in file list.
        return test.waitForElement(googleDrive);
      })
      .then(() => {
        assertTrue(test.fakeMouseClick(googleDrive), 'click Google Drive');
        return test.waitForFiles(
            test.TestEntryInfo.getExpectedRows(test.BASIC_DRIVE_ENTRY_SET));
      })
      .then(() => {
        // Check 'Share with Linux' is not shown in menu.
        test.selectFile('photos');
        assertTrue(
            test.fakeMouseRightClick('#file-list li[selected]'),
            'right-click photos');
        return test.waitForElement(menuNoShareWithLinux);
      })
      .then(() => {
        // Reset Linux files back to unmounted.
        chrome.fileManagerPrivate.removeMount('crostini');
        return test.waitForElement(
            '#directory-tree .tree-item [root-type-icon="crostini"]');
      })
      .then(() => {
        done();
      });
};
