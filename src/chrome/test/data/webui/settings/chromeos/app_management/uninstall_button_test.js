// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {AppManagementStore, FakePageHandler, updateSelectedAppId, PageType} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {setupFakeHandler, replaceStore, replaceBody, isHiddenByDomIf, isHidden} from './test_util.m.js';
// #import {flushTasks} from 'chrome://test/test_util.js';
// clang-format on

'use strict';

suite('<app-management-uninstall-button', () => {
  let uninstallButton;
  let fakeHandler;
  let app;


  setup(async () => {
    fakeHandler = setupFakeHandler();
    replaceStore();
  });

  async function setupUninstallButton(installReason) {
    // Create an ARC app options.
    const arcOptions = {
      type: apps.mojom.AppType.kArc,
      installReason: installReason
    };

    // Add an app, and make it the currently selected app.
    app = await fakeHandler.addApp('app1_id', arcOptions);
    app_management.AppManagementStore.getInstance().dispatch(
        app_management.actions.updateSelectedAppId(app.id));
    await fakeHandler.flushPipesForTesting();

    assertTrue(
        !!app_management.AppManagementStore.getInstance().data.apps[app.id]);

    uninstallButton = document.createElement('app-management-uninstall-button');

    replaceBody(uninstallButton);
    await fakeHandler.flushPipesForTesting();
  }

  test('Click uninstall', async () => {
    await setupUninstallButton(apps.mojom.InstallReason.kUser);

    uninstallButton.$$('#uninstallButton').click();
    await fakeHandler.flushPipesForTesting();
    assertFalse(
        !!app_management.AppManagementStore.getInstance().data.apps[app.id]);
  });

  test('Disabled by policy', async () => {
    await setupUninstallButton(apps.mojom.InstallReason.kPolicy);
    uninstallButton.$$('#uninstallButton').click();
    await fakeHandler.flushPipesForTesting();
    // Disabled by policy, clicking should not remove app.
    assertTrue(
        !!app_management.AppManagementStore.getInstance().data.apps[app.id]);
  });

  test('System app, button hidden', async () => {
    await setupUninstallButton(apps.mojom.InstallReason.kSystem);
    assertFalse(!!uninstallButton.$$('#uninstallButton'));
    await fakeHandler.flushPipesForTesting();
    // Disabled by policy, clicking should not remove app.
    assertTrue(
        !!app_management.AppManagementStore.getInstance().data.apps[app.id]);
  });
});
