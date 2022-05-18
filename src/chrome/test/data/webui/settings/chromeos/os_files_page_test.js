// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/chromeos/lazy_load.js';

import {Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {waitAfterNextRender} from 'chrome://test/test_util.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';

suite('FilesPageTests', function() {
  /** @type {SettingsFilesPageElement} */
  let filesPage = null;

  setup(function() {
    PolymerTest.clearBody();
    filesPage = document.createElement('os-settings-files-page');
    document.body.appendChild(filesPage);
    flush();
  });

  teardown(function() {
    filesPage.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('Disconnect Google Drive account,pref disabled/enabled', async () => {
    // The default state of the pref is disabled.
    const disconnectGoogleDrive = assert(
        filesPage.shadowRoot.querySelector('#disconnectGoogleDriveAccount'));
    assertFalse(disconnectGoogleDrive.checked);

    disconnectGoogleDrive.shadowRoot.querySelector('cr-toggle').click();
    flush();
    assertTrue(disconnectGoogleDrive.checked);
  });

  test('Smb Shares, Navigates to SMB_SHARES route on click', async () => {
    const smbShares = assert(filesPage.shadowRoot.querySelector('#smbShares'));

    smbShares.click();
    flush();
    assertEquals(Router.getInstance().getCurrentRoute(), routes.SMB_SHARES);
  });

  test('Deep link to disconnect Google Drive', async () => {
    const params = new URLSearchParams();
    params.append('settingId', '1300');
    Router.getInstance().navigateTo(routes.FILES, params);

    flush();

    const deepLinkElement =
        filesPage.shadowRoot.querySelector('#disconnectGoogleDriveAccount')
            .shadowRoot.querySelector('cr-toggle');
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Disconnect Drive toggle should be focused for settingId=1300.');
  });
});
