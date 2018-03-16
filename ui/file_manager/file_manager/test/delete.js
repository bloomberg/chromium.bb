// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testDeleteMenuItemIsDisabledWhenNoItemIsSelected(done) {
  setupAndWaitUntilReady()
      .then(detailTable => {
        return waitForElement('list.list');
      })
      .then(emptySpace => {
        assertTrue(test.util.sync.fakeMouseRightClick(window, 'list.list'));
        return waitForElement('#file-context-menu:not([hidden])');
      })
      .then(result => {
        return waitForElement(
            'cr-menu-item[command="#delete"][disabled="disabled"]');
      })
      .then(result => {
        done();
      })
      .catch(err => {
        console.error(err);
        done(true);
      });
}

function testDeleteOneItemFromToolbar(done) {
  var beforeDeletion = TestEntryInfo.getExpectedRows([
      ENTRIES.photos,
      ENTRIES.hello,
      ENTRIES.world,
      ENTRIES.desktop,
      ENTRIES.beautiful
  ]);

  var afterDeletion = TestEntryInfo.getExpectedRows([
      ENTRIES.photos,
      ENTRIES.hello,
      ENTRIES.world,
      ENTRIES.beautiful
  ]);

  setupAndWaitUntilReady()
      .then(detailTable => {
        return waitForFiles(beforeDeletion);
      })
      .then(result => {
        return test.util.sync.selectFile(window, 'My Desktop Background.png');
      })
      .then(result => {
        assertTrue(result);
        return test.util.sync.fakeMouseClick(window, 'button#delete-button');
      })
      .then(result => {
        assertTrue(result);
        return waitForElement('.cr-dialog-container.shown');
      })
      .then(result => {
        return test.util.sync.fakeMouseClick(window, 'button.cr-dialog-ok');
      })
      .then(result => {
        assertTrue(result);
        return waitForFiles(afterDeletion);
      })
      .then(result => {
        done();
      })
      .catch(err => {
        console.error(err);
        done(err);
      });
}
