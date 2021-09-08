// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertInstanceof} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {assertFalse, assertTrue} from 'chrome://test/chai_assert.js';
import {InstallLinuxPackageDialog} from './install_linux_package_dialog.m.js';

export function testInstallButtonHiddenUntilInfoReady() {
  // Polyfill chrome.app.window.current().
  /** @suppress {checkTypes,const} */
  chrome.app = {window: {current: () => null}};

  // Mock loadTimeData.
  loadTimeData.getString = id => id;
  loadTimeData.data = {};

  let getInfoCallback;
  /** @suppress {checkTypes,const} */
  chrome.fileManagerPrivate = {
    getLinuxPackageInfo: (entry, callback) => {
      getInfoCallback = callback;
    }
  };

  const info = {name: 'n', version: 'v', info: 'i', summary: 's'};
  const dialog = new InstallLinuxPackageDialog(document.body);

  // Show dialog and verify that the install button is disabled.
  dialog.showInstallLinuxPackageDialog(/** @type {!Entry} */ ({}));
  const installButton = document.querySelector('.cr-dialog-ok');
  assertTrue(installButton.disabled);

  // The install button should become enabled once info is ready.
  getInfoCallback(info);
  assertFalse(installButton.disabled);
}
