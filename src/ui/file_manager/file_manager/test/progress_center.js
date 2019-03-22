// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const progressCenter = {};

/**
 * @param {string} id
 * @param {string} message
 * @return {!ProgressCenterItem}
 */
progressCenter.createItem = function(id, message) {
  let item = new ProgressCenterItem();
  item.id = id;
  item.message = message;
  return item;
};

progressCenter.testScrollWhenManyMessages = (done) => {
  const visibleClosed = '#progress-center:not([hidden]):not(.opened)';
  const visibleOpen = '#progress-center:not([hidden]).opened';
  const openIcon = '#progress-center-close-view .open';
  const navListFooter = '.dialog-navigation-list-footer';

  const items = [];
  const center = fileManager.fileBrowserBackground_.progressCenter;
  // Load a single file.
  test.setupAndWaitUntilReady()
      .then(() => {
        // Add lots of messages.
        for (let i = 0; i < 100; i++) {
          const item = progressCenter.createItem('id' + i, 'msg ' + i);
          items.push(item);
          center.updateItem(item);
        }
        // Wait for notification expand icon.
        return test.waitForElement(visibleClosed);
      })
      .then(result => {
        // Click open icon, ensure progress center is open.
        assertTrue(test.fakeMouseClick(openIcon));
        return test.waitForElement(visibleOpen);
      })
      .then(result => {
        // Ensure progress center is scrollable.
        const footer = document.querySelector(navListFooter);
        assertTrue(footer.scrollHeight > footer.clientHeight);

        // Clear items.
        items.forEach((item) => {
          item.state = ProgressItemState.COMPLETED;
          center.updateItem(item);
        });
        done();
      });
};
