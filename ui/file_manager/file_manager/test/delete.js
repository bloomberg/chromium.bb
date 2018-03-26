// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testDeleteMenuItemIsDisabledWhenNoItemIsSelected(done) {
  test.setupAndWaitUntilReady()
      .then(() => {
        return test.waitForElement('list.list');
      })
      .then(() => {
        assertTrue(test.fakeMouseRightClick('list.list'));
        return test.waitForElement('#file-context-menu:not([hidden])');
      })
      .then(() => {
        return test.waitForElement(
            'cr-menu-item[command="#delete"][disabled="disabled"]');
      })
      .then(() => {
        // Click back on empty list to close #file-context-menu.
        assertTrue(test.fakeMouseClick('list.list'));
        done();
      })
      .catch(err => {
        console.error(err);
        done(true);
      });
}

function testDeleteOneItemFromToolbar(done) {
  var beforeDeletion = test.TestEntryInfo.getExpectedRows([
    test.ENTRIES.photos,
    test.ENTRIES.hello,
    test.ENTRIES.world,
    test.ENTRIES.desktop,
    test.ENTRIES.beautiful,
  ]);

  var afterDeletion = test.TestEntryInfo.getExpectedRows([
    test.ENTRIES.photos,
    test.ENTRIES.hello,
    test.ENTRIES.world,
    test.ENTRIES.beautiful,
  ]);

  test.setupAndWaitUntilReady()
      .then(detailTable => {
        return test.waitForFiles(beforeDeletion);
      })
      .then(result => {
        return test.selectFile('My Desktop Background.png');
      })
      .then(result => {
        assertTrue(result);
        return test.fakeMouseClick('button#delete-button');
      })
      .then(result => {
        assertTrue(result);
        return test.waitForElement('.cr-dialog-container.shown');
      })
      .then(result => {
        return test.fakeMouseClick('button.cr-dialog-ok');
      })
      .then(result => {
        assertTrue(result);
        return test.waitForFiles(afterDeletion);
      })
      .then(result => {
        done();
      })
      .catch(err => {
        console.error(err);
        done(err);
      });
}
