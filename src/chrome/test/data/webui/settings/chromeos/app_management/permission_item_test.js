// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {AppManagementStore, FakePageHandler, PermissionType, updateSelectedAppId, getPermissionValueBool} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {setupFakeHandler, replaceStore, replaceBody} from './test_util.m.js';
// #import {flushTasks} from 'chrome://test/test_util.js';
// clang-format on

'use strict';

suite('<app-management-permission-item>', () => {
  let permissionItem;
  let fakeHandler;

  setup(async () => {
    fakeHandler = setupFakeHandler();
    replaceStore();

    const arcOptions = {
      type: apps.mojom.AppType.kArc,
      permissions: app_management.FakePageHandler.createArcPermissions([
        PermissionType.kCamera,
        PermissionType.kLocation,
        PermissionType.kNotifications,
        PermissionType.kContacts,
        PermissionType.kStorage,
      ])
    };

    // Add an arc app, and make it the currently selected app.
    const app = await fakeHandler.addApp(null, arcOptions);
    app_management.AppManagementStore.getInstance().dispatch(
        app_management.actions.updateSelectedAppId(app.id));

    permissionItem = document.createElement('app-management-permission-item');
  });

  test('Toggle permission', async () => {
    permissionItem.permissionType = 'kLocation';

    replaceBody(permissionItem);
    await fakeHandler.flushPipesForTesting();
    assertTrue(app_management.util.getPermissionValueBool(
        permissionItem.app_, permissionItem.permissionType));

    permissionItem.click();
    await test_util.flushTasks();
    await fakeHandler.flushPipesForTesting();
    assertFalse(app_management.util.getPermissionValueBool(
        permissionItem.app_, permissionItem.permissionType));
  });
});
