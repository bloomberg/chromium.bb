// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const crostiniTasks = {};

crostiniTasks.testShareBeforeOpeningDownloadsWithCrostiniApp = (done) => {
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

crostiniTasks.testErrorOpeningDownloadsRootWithDefaultCrostiniApp = (done) => {
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

crostiniTasks.testErrorLoadingLinuxPackageInfo = (done) => {
  const linuxFiles = '#directory-tree .tree-item [root-type-icon="crostini"]';
  const dialog = '#install-linux-package-dialog';
  const detailsFrame = '.install-linux-package-details-frame';

  // Save old fmp.getFileTasks and replace with version that returns
  // the internal linux package install handler
  let oldGetFileTasks = chrome.fileManagerPrivate.getFileTasks;
  chrome.fileManagerPrivate.getFileTasks = (entries, callback) => {
    setTimeout(callback, 0, [{
                 taskId: test.FILE_MANAGER_EXTENSION_ID +
                     '|file|install-linux-package',
                 title: 'Install with Linux (Beta)',
                 verb: 'open_with',
                 isDefault: true,
               }]);
  };

  // Save old fmp.getLinuxPackageInfo and replace with version that saves the
  // callback to manually call later
  let oldGetLinuxPackageInfo = chrome.fileManagerPrivate.getLinuxPackageInfo;
  let packageInfoCallback = null;
  chrome.fileManagerPrivate.getLinuxPackageInfo = (entry, callback) => {
    packageInfoCallback = callback;
  };

  test.setupAndWaitUntilReady([], [], [test.ENTRIES.debPackage])
      .then(() => {
        return test.waitForElement(linuxFiles);
      })
      .then(() => {
        // Select 'Linux files' in directory tree.
        assertTrue(test.fakeMouseClick(linuxFiles), 'click Linux files');
        return test.waitForFiles(
            test.TestEntryInfo.getExpectedRows([test.ENTRIES.debPackage]));
      })
      .then(() => {
        // Double click on 'package.deb' file to open the install dialog.
        assertTrue(test.fakeMouseDoubleClick('[file-name="package.deb"]'));
        return test.waitForElement(dialog);
      })
      .then(() => {
        // Verify the loading state is shown.
        assertEquals(
            'Details\nLoading information...',
            document.querySelector(detailsFrame).innerText);
        return test.repeatUntil(() => {
          return packageInfoCallback ||
              test.pending('Waiting for package info request');
        });
      })
      .then(() => {
        // Call the callback with an error.
        chrome.runtime.lastError = {message: 'error message'};
        packageInfoCallback(undefined);
        delete chrome.runtime.lastError;
        assertEquals(
            'Details\nFailed to retrieve app info.',
            document.querySelector(detailsFrame).innerText);

        // Click 'cancel' to close.  Ensure dialog closes.
        assertTrue(test.fakeMouseClick('button.cr-dialog-cancel'));
        return test.waitForElementLost(dialog);
      })
      .then(() => {
        // Restore fmp.getFileTasks, fmp.getLinuxPackageInfo.
        chrome.fileManagerPrivate.getFileTasks = oldGetFileTasks;
        chrome.fileManagerPrivate.getLinuxPackageInfo = oldGetLinuxPackageInfo;
        done();
      });
};
