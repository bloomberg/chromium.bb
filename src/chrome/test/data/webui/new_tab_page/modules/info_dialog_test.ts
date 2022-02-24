// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';

import {InfoDialogElement} from 'chrome://new-tab-page/new_tab_page.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('NewTabPageModulesInfoDialogTest', () => {
  let infoDialog: InfoDialogElement;

  setup(() => {
    document.body.innerHTML = '';
    infoDialog = new InfoDialogElement();
    document.body.appendChild(infoDialog);
  });

  test('can open dialog', () => {
    assertFalse(infoDialog.$.dialog.open);
    infoDialog.showModal();
    assertTrue(infoDialog.$.dialog.open);
  });

  test('clicking close button closes cr dialog', () => {
    // Arrange.
    infoDialog.showModal();

    // Act.
    infoDialog.$.closeButton.click();

    // Assert.
    assertFalse(infoDialog.$.dialog.open);
  });
});
