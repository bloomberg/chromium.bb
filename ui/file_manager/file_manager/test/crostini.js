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
  const oldMount = chrome.fileManagerPrivate.mountCrostini;
  let mountCallback = null;
  chrome.fileManagerPrivate.mountCrostini = (callback) => {
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
        // Ensure mountCrostini is called.
        return test.repeatUntil(() => {
          if (!mountCallback)
            return test.pending('Waiting for mountCrostini');
          return mountCallback;
        });
      })
      .then(() => {
        // Intercept the fileManagerPrivate.mountCrostini call
        // and add crostini disk mount.
        test.mountCrostini();
        // Continue from fileManagerPrivate.mountCrostini callback
        // and ensure expected files are shown.
        mountCallback();
        return test.waitForFiles(
            test.TestEntryInfo.getExpectedRows(test.BASIC_CROSTINI_ENTRY_SET));
      })
      .then(() => {
        // Reset fileManagerPrivate.mountCrostini and remove mount.
        chrome.fileManagerPrivate.mountCrostini = oldMount;
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
  const oldMount = chrome.fileManagerPrivate.mountCrostini;
  // Override fileManagerPrivate.mountCrostini to return error.
  chrome.fileManagerPrivate.mountCrostini = (callback) => {
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
        return test.waitForElementLost('.cr-dialog-container');
      })
      .then(() => {
        // Reset chrome.fileManagerPrivate.mountCrostini.
        chrome.fileManagerPrivate.mountCrostini = oldMount;
        done();
      });
};

crostini.testCrostiniMountOnDrag = (done) => {
  chrome.fileManagerPrivate.mountCrostiniDelay_ = 0;
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

crostini.testShareBeforeOpeningDownloadsWithCrostiniApp = (done) => {
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

  // Save old fmp.sharePathWitCrostini.
  const oldSharePath = chrome.fileManagerPrivate.sharePathWithCrostini;
  let sharePathCalled = false;
  chrome.fileManagerPrivate.sharePathWithCrostini = (entry, callback) => {
    sharePathCalled = true;
    oldSharePath(entry, callback);
  };

  // Save old fmp.executeTask.
  const oldExecuteTask = chrome.fileManagerPrivate.executeTask;
  let executeTaskCalled = false;
  chrome.fileManagerPrivate.executeTask = (taskId, entries, callback) => {
    executeTaskCalled = true;
    oldExecuteTask(taskId, entries, callback);
  };
  chrome.metricsPrivate.values_ = [];

  test.setupAndWaitUntilReady([], [], [])
      .then(() => {
        // Add '/A', and '/A/hello.txt', refresh, 'A' is shown.
        test.addEntries(
            [test.ENTRIES.directoryA, test.ENTRIES.helloInA], [], []);
        assertTrue(test.fakeMouseClick('#refresh-button'), 'click refresh');
        return test.waitForFiles(
            test.TestEntryInfo.getExpectedRows([test.ENTRIES.directoryA]));
      })
      .then(() => {
        // Change to 'A' directory, hello.txt is shown.
        assertTrue(test.fakeMouseDoubleClick('[file-name="A"]'));
        return test.waitForFiles(
            test.TestEntryInfo.getExpectedRows([test.ENTRIES.hello]));
      })
      .then(() => {
        // Right click on 'hello.txt' file, wait for dialog with 'Open with'.
        assertTrue(test.fakeMouseRightClick('[file-name="hello.txt"]'));
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
        // share path dialog is shown.
        const list = document.querySelectorAll('#default-tasks-list li div');
        assertEquals(2, list.length);
        assertEquals('Open with Crostini App', list[0].innerText);
        assertEquals('Open with Text', list[1].innerText);
        assertTrue(test.fakeMouseClick('#default-tasks-list li'));
        return test.repeatUntil(() => {
          return document.querySelector('.cr-dialog-title').innerText ===
              'Share files with Linux' ||
              test.pending('Waiting for share before open dialog');
        });
      })
      .then(() => {
        // Validate dialog messages, click 'OK' to share and open.  Ensure
        // dialog closes.
        assertEquals(
            'Let Linux apps open <b>hello.txt</b>.',
            document.querySelector('.cr-dialog-text').innerHTML);
        assertTrue(test.fakeMouseClick('button.cr-dialog-ok'));
        return test.waitForElementLost('.cr-dialog-container');
      })
      .then(() => {
        // Ensure fmp.sharePathWithCrostini, fmp.executeTask called.
        return test.repeatUntil(() => {
          return sharePathCalled && executeTaskCalled ||
              test.pending('Waiting to share and open');
        });
      })
      .then(() => {
        // Validate UMAs.
        const lastEnumUma = chrome.metricsPrivate.values_.pop();
        assertEquals(
            'FileBrowser.CrostiniShareDialog', lastEnumUma[0].metricName);
        assertEquals(1 /* ShareBeforeOpen */, lastEnumUma[1]);

        // Restore fmp.*.
        chrome.fileManagerPrivate.getFileTasks = oldGetFileTasks;
        chrome.fileManagerPrivate.sharePathWithCrostini = oldSharePath;
        chrome.fileManagerPrivate.executeTask = oldExecuteTask;
        done();
      });
};

crostini.testErrorOpeningDownloadsRootWithDefaultCrostiniApp = (done) => {
  // Save old fmp.getFileTasks and replace with version that returns
  // crostini app and chrome Text app.
  let oldGetFileTasks = chrome.fileManagerPrivate.getFileTasks;
  chrome.fileManagerPrivate.getFileTasks = (entries, callback) => {
    setTimeout(callback, 0, [{
                 taskId: 'crostini-app-id|crostini|open-with',
                 title: 'Crostini App',
                 verb: 'open_with',
                 isDefault: true,
               }]);
  };

  test.setupAndWaitUntilReady()
      .then(() => {
        // Right click on 'world.ogv' file, wait for dialog with the default
        // task action.
        assertTrue(test.fakeMouseRightClick('[file-name="world.ogv"]'));
        return test.repeatUntil(() => {
          return document
                     .querySelector(
                         'cr-menu-item[command="#default-task"]:not([hidden])')
                     .label === 'Crostini App' ||
              test.pending('Waiting for default task menu item');
        });
      })
      .then(() => {
        // Click 'Open with', wait for picker.
        assertTrue(
            test.fakeMouseClick('cr-menu-item[command="#default-task"]'));
        return test.waitForElement('.cr-dialog-container');
      })
      .then(() => {
        // Validate error messages, click 'OK' to close.  Ensure dialog closes.
        assertEquals(
            'Unable to open with Crostini App',
            document.querySelector('.cr-dialog-title').innerText);
        assertEquals(
            'To open files with Crostini App, ' +
                'first copy to Linux files folder.',
            document.querySelector('.cr-dialog-text').innerText);
        assertTrue(test.fakeMouseClick('button.cr-dialog-ok'));
        return test.waitForElementLost('.cr-dialog-container');
      })
      .then(() => {
        // Restore fmp.getFileTasks.
        chrome.fileManagerPrivate.getFileTasks = oldGetFileTasks;
        done();
      });
};

crostini.testSharePathCrostiniSuccess = (done) => {
  const oldSharePath = chrome.fileManagerPrivate.sharePathWithCrostini;
  let sharePathCalled = false;
  chrome.fileManagerPrivate.sharePathWithCrostini = (entry, callback) => {
    oldSharePath(entry, () => {
      sharePathCalled = true;
      callback();
    });
  };
  chrome.metricsPrivate.smallCounts_ = [];
  chrome.metricsPrivate.values_ = [];

  test.setupAndWaitUntilReady()
      .then(() => {
        // Right-click 'photos' directory.
        // Check 'Share with Linux' is shown in menu.
        assertTrue(
            test.fakeMouseRightClick('#file-list [file-name="photos"]'),
            'right-click photos');
        return test.waitForElement(
            '#file-context-menu:not([hidden]) ' +
            '[command="#share-with-linux"]:not([hidden]):not([disabled])');
      })
      .then(() => {
        // Click on 'Share with Linux'.
        assertTrue(
            test.fakeMouseClick(
                '#file-context-menu [command="#share-with-linux"]'),
            'Share with Linux');
        // Check sharePathWithCrostini is called.
        return test.repeatUntil(() => {
          return sharePathCalled || test.pending('wait for sharePathCalled');
        });
      })
      .then(() => {
        // Validate UMAs.
        assertEquals(1, chrome.metricsPrivate.smallCounts_.length);
        assertArrayEquals(
            ['FileBrowser.CrostiniSharedPaths.Depth.downloads', 1],
            chrome.metricsPrivate.smallCounts_[0]);
        const lastEnumUma = chrome.metricsPrivate.values_.pop();
        assertEquals('FileBrowser.MenuItemSelected', lastEnumUma[0].metricName);
        assertEquals(12 /* Share with Linux */, lastEnumUma[1]);

        // Restore fmp.*.
        chrome.fileManagerPrivate.sharePathWithCrostini = oldSharePath;
        done();
      });
};

// Verify right-click menu with 'Share with Linux' is not shown for:
// * Files (not directory)
// * Any folder already shared
// * Root Downloads folder
// * Any folder outside of downloads (e.g. crostini or orive)
crostini.testSharePathNotShown = (done) => {
  const myFiles = '#directory-tree .tree-item [root-type-icon="my_files"]';
  const downloads = '#file-list li [file-type-icon="downloads"]';
  const linuxFiles = '#directory-tree .tree-item [root-type-icon="crostini"]';
  const googleDrive = '#directory-tree .tree-item [volume-type-icon="drive"]';
  const menuNoShareWithLinux = '#file-context-menu:not([hidden]) ' +
      '[command="#share-with-linux"][hidden][disabled="disabled"]';
  let alreadySharedPhotosDir;

  test.setupAndWaitUntilReady()
      .then(() => {
        // Right-click 'hello.txt' file.
        // Check 'Share with Linux' is not shown in menu.
        assertTrue(
            test.fakeMouseRightClick('#file-list [file-name="hello.txt"]'),
            'right-click hello.txt');
        return test.waitForElement(menuNoShareWithLinux);
      })
      .then(() => {
        // Set a folder as already shared.
        alreadySharedPhotosDir =
            mockVolumeManager
                .getCurrentProfileVolumeInfo(
                    VolumeManagerCommon.VolumeType.DOWNLOADS)
                .fileSystem.entries['/photos'];
        Crostini.registerSharedPath(alreadySharedPhotosDir, mockVolumeManager);
        assertTrue(
            test.fakeMouseRightClick('#file-list [file-name="photos"]'),
            'right-click hello.txt');
        return test.waitForElement(menuNoShareWithLinux);
      })
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
        assertTrue(
            test.fakeMouseRightClick('#file-list [file-name="A"]'),
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
        assertTrue(
            test.fakeMouseRightClick('#file-list [file-name="photos"]'),
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
        // Clear Crostini shared folders.
        Crostini.unregisterSharedPath(
            alreadySharedPhotosDir, mockVolumeManager);
        done();
      });
};
