// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Suite of tests for extension options dialog.
 * These are run as part of interactive_ui_tests.
 */
suite('ExtensionOptionsDialogTest', () => {
  test('show options dialog', async () => {
    const manager = document.querySelector('extensions-manager');
    assertTrue(!!manager);
    const extensionDetailView = manager.$$('extensions-detail-view');
    assertTrue(!!extensionDetailView);

    const optionsButton = extensionDetailView.$$('#extensions-options');
    optionsButton.click();
    await test_util.eventToPromise('cr-dialog-open', manager);
    const dialog = manager.$$('#options-dialog');
    let waitForClose = test_util.eventToPromise('close', dialog);
    dialog.$.dialog.cancel();
    await waitForClose;
    const activeElement = getDeepActiveElement();
    assertEquals('CR-ICON-BUTTON', activeElement.tagName);
    assertEquals(optionsButton.$.icon, getDeepActiveElement());
  });
});
