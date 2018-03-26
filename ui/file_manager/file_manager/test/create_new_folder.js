// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Selects the first item in the file list.
 * @return {Promise} Promise to be fulfilled on success.
 */
test.selectFirstListItem = function() {
  return Promise.resolve()
      .then(() => {
        // Ensure no selected item.
        return test.waitForElementLost(
            'div.detail-table > list > li[selected]');
      })
      .then(() => {
        // Push Down.
        assertTrue(test.fakeKeyDown(
            '#file-list', 'ArrowDown', 'Down', true, false, false));
        // Wait for selection.
        return test.waitForElement('div.detail-table > list > li[selected]');
      })
      .then(() => {
        // Ensure that only the first item is selected.
        var elements =
            document.querySelectorAll('div.detail-table > list > li[selected]');
        assertEquals(1, elements.length);
        assertEquals('listitem-' + test.minListItemId(), elements[0].id);
        return elements[0];
      });
};

/**
 * Creates new folder.
 * @param {Array<TestEntryInfo>} initialEntrySet Initial set of entries.
 * @return {Promise} Promise to be fulfilled on success.
 */
test.createNewFolder = function(initialEntrySet) {
  var maxListItemId = test.maxListItemId();
  return Promise.resolve()
      .then(() => {
        // Push Ctrl + E.
        assertTrue(
            test.fakeKeyDown('#file-list', 'e', 'U+0045', true, false, false));
        // Wait for rename text field.
        return test.waitForElement('li[renaming] input.rename');
      })
      .then(() => {
        var elements =
            document.querySelectorAll('div.detail-table > list > li[selected]');
        // Ensure that only the new directory is selected and being renamed.
        assertTrue(elements.length == 1);
        assertTrue('renaming' in elements[0].attributes);

        // Check directory tree for new folder.
        return test.waitForElement('#listitem-' + (maxListItemId + 1));
      })
      .then(() => {
        // Type new folder name.
        test.inputText('input.rename', 'Test Folder Name');
        // Push Enter.
        assertTrue(test.fakeKeyDown(
            'input.rename', 'Enter', 'Enter', false, false, false));

        // Wait until rename completes.
        return test.waitForElementLost('input.rename');
      })
      .then(() => {
        // Once it is renamed, the original 'New Folder' item is removed.
        return test.waitForElementLost('#listitem-' + (maxListItemId + 1));
      })
      .then(() => {
        // A newer entry is then added for the renamed folder.
        if (chrome.extension.inIncognitoContext)
          return test.waitForElement('#listitem-' + (maxListItemId + 2));
      })
      .then(() => {
        var expectedEntryRows =
            test.TestEntryInfo.getExpectedRows(initialEntrySet);
        expectedEntryRows.push(['Test Folder Name', '--', 'Folder', '']);
        // Wait for the new folder.
        return test.waitForFiles(expectedEntryRows, {ignoreDate: true});
      })
      .then(() => {
        // Wait until the new created folder is selected.
        var nameSpanQuery = 'div.detail-table > list > ' +
            'li[selected]:not([renaming]) span.entry-name';

        return test.repeatUntil(() => {
          var elements = document.querySelectorAll(
              'div.detail-table > list > li[selected] span.entry-name');

          if (elements.length !== 1) {
            return test.pending(
                'Selection is not ready (elements: %j)', elements);
          } else if (elements[0].textContent !== 'Test Folder Name') {
            return test.pending(
                'Selected item is wrong. (actual: %s)', elements[0].text);
          } else {
            return true;
          }
        });
      });
};

function testCreateNewFolderAfterSelectFile(done) {
  test.setupAndWaitUntilReady()
      .then(() => {
        return test.selectFirstListItem();
      })
      .then(() => {
        return test.createNewFolder(test.BASIC_LOCAL_ENTRY_SET);
      })
      .then(() => {
        done();
      })
      .catch(err => {
        console.error(err);
        done(true);
      });
}

function testCreateNewFolderDownloads(done) {
  test.setupAndWaitUntilReady()
      .then(() => {
        return test.createNewFolder(test.BASIC_LOCAL_ENTRY_SET);
      })
      .then(() => {
        done();
      })
      .catch(err => {
        console.error(err);
        done(true);
      });
}

function testCreateNewFolderDrive(done) {
  test.setupAndWaitUntilReady()
      .then(() => {
        assertTrue(
            test.fakeMouseClick('#directory-tree [volume-type-icon="drive"]'));
        return test.waitForFiles(
            test.TestEntryInfo.getExpectedRows(test.BASIC_DRIVE_ENTRY_SET));
      })
      .then(() => {
        return test.createNewFolder(test.BASIC_DRIVE_ENTRY_SET);
      })
      .then(() => {
        done();
      })
      .catch(err => {
        console.error(err);
        done(true);
      });
}
