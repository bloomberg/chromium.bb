// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const checkselect = {};

checkselect.testCancelCheckSelectModeAfterAction = (done) => {
  test.setupAndWaitUntilReady()
      .then(() => {
        // Click 2nd last file on checkmark to start check-select-mode.
        assertTrue(test.fakeMouseClick(
            '#file-list li.table-row:nth-of-type(4) .detail-checkmark'));
        return test.waitForElement(
            '#file-list li[selected].table-row:nth-of-type(4)');
      })
      .then(result => {
        // Click last file on checkmark, adds to selection.
        assertTrue(test.fakeMouseClick(
            '#file-list li.table-row:nth-of-type(5) .detail-checkmark'));
        return test.waitForElement(
            '#file-list li[selected].table-row:nth-of-type(5)');
      })
      .then(result => {
        assertEquals(
            2, document.querySelectorAll('#file-list li[selected').length);
        // Click selection menu (3-dots).
        assertTrue(test.fakeMouseClick('#selection-menu-button'));
        return test.waitForElement(
            '#file-context-menu:not([hidden]) ' +
            'cr-menu-item[command="#cut"]:not([disabled])');
      })
      .then(result => {
        // Click 'cut'.
        test.fakeMouseClick('#file-context-menu cr-menu-item[command="#cut"]');
        return test.waitForElement('#file-context-menu[hidden]');
      })
      .then(result => {
        // Click first photos dir in checkmark and make sure 4 and 5 not
        // selected.
        assertTrue(test.fakeMouseClick(
            '#file-list li.table-row:nth-of-type(1) .detail-checkmark'));
        return test.waitForElement(
            '#file-list li[selected].table-row:nth-of-type(1)');
      })
      .then(result => {
        assertEquals(
            1, document.querySelectorAll('#file-list li[selected]').length);
        done();
      });
};

checkselect.testCheckSelectModeAfterSelectAllOneFile = (done) => {
  const gearMenu = document.querySelector('#gear-menu');
  const cancel = document.querySelector('#cancel-selection-button-wrapper');
  const selectAll =
      '#gear-menu:not([hidden]) #gear-menu-select-all:not([disabled])';

  // Load a single file.
  test.setupAndWaitUntilReady([test.ENTRIES.hello])
      .then(() => {
        // Click gear menu, ensure 'Select all' is shown.
        assertTrue(test.fakeMouseClick('#gear-button'));
        return test.waitForElement(selectAll);
      })
      .then(result => {
        // Click 'Select all', gear menu now replaced with file context menu.
        assertTrue(test.fakeMouseClick('#gear-menu-select-all'));
        return test.repeatUntil(() => {
          return getComputedStyle(gearMenu).opacity == 0 &&
              getComputedStyle(cancel).display == 'block' ||
              test.pending('waiting for check select mode from click');
        });
      })
      .then(result => {
        // Cancel selection, ensure no items selected.
        assertTrue(test.fakeMouseClick('#cancel-selection-button'));
        return test.repeatUntil(() => {
          return document.querySelectorAll('#file-list li[selected]').length ==
              0 ||
              test.pending('waiting for no files selected after click');
        });
      })
      .then(result => {
        // 'Ctrl+a' to select all.
        assertTrue(test.fakeKeyDown('#file-list', 'a', true, false, false));
        return test.repeatUntil(() => {
          return getComputedStyle(cancel).display == 'block' ||
              test.pending('waiting for check select mode from key');
        });
      })
      .then(result => {
        // Cancel selection, ensure no items selected.
        assertTrue(test.fakeMouseClick('#cancel-selection-button'));
        return test.repeatUntil(() => {
          return document.querySelectorAll('#file-list li[selected]').length ==
              0 ||
              test.pending('waiting for no files selected after key');
        });
      })
      .then(result => {
        done();
      });
};