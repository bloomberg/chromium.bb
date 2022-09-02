// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

import {AppManagementStore, FakePageHandler, PermissionType, updateSelectedAppId, getPermissionValueBool} from 'chrome://os-settings/chromeos/os_settings.js';
import {setupFakeHandler, replaceStore, replaceBody, isHiddenByDomIf, isHidden, getPermissionItemByType, getPermissionCrToggleByType} from './test_util.js';
import {flushTasks} from 'chrome://test/test_util.js';

suite('<app-management-arc-detail-view>', () => {
  let arcPermissionView;
  let fakeHandler;

  function expandPermissions() {
    arcPermissionView.root.querySelector('#subpermission-expand-row').click();
  }

  function getPermissionBoolByType(permissionType) {
    return getPermissionValueBool(arcPermissionView.app_, permissionType);
  }

  async function clickPermissionToggle(permissionType) {
    getPermissionCrToggleByType(arcPermissionView, permissionType).click();
    await fakeHandler.flushPipesForTesting();
  }

  async function clickPermissionItem(permissionType) {
    getPermissionItemByType(arcPermissionView, permissionType).click();
    await fakeHandler.flushPipesForTesting();
  }

  setup(async () => {
    fakeHandler = setupFakeHandler();
    replaceStore();

    // Create an ARC app without microphone permissions.
    const arcOptions = {
      type: appManagement.mojom.AppType.kArc,
      permissions: FakePageHandler.createArcPermissions([
        PermissionType.kCamera,
        PermissionType.kLocation,
        PermissionType.kNotifications,
        PermissionType.kContacts,
        PermissionType.kStorage,
      ])
    };

    // Add an arc app, and make it the currently selected app.
    const app = await fakeHandler.addApp(null, arcOptions);
    AppManagementStore.getInstance().dispatch(updateSelectedAppId(app.id));

    arcPermissionView =
        document.createElement('app-management-arc-detail-view');
    replaceBody(arcPermissionView);
  });

  test('App is rendered correctly', () => {
    assertEquals(
        AppManagementStore.getInstance().data.selectedAppId,
        arcPermissionView.app_.id);
  });

  test('Permissions are hidden correctly', () => {
    expandPermissions();
    assertTrue(
        isHidden(getPermissionItemByType(arcPermissionView, 'kMicrophone')));
    assertFalse(
        isHidden(getPermissionItemByType(arcPermissionView, 'kLocation')));
    assertFalse(
        isHidden(getPermissionItemByType(arcPermissionView, 'kCamera')));
    assertFalse(
        isHidden(getPermissionItemByType(arcPermissionView, 'kStorage')));
    assertFalse(
        isHidden(getPermissionItemByType(arcPermissionView, 'kCamera')));
  });

  test('Toggle works correctly', async () => {
    const checkPermissionToggle = async (permissionType) => {
      assertTrue(getPermissionBoolByType(permissionType));
      assertTrue(getPermissionCrToggleByType(arcPermissionView, permissionType)
                     .checked);

      // Toggle Off.
      await clickPermissionToggle(permissionType);
      assertFalse(getPermissionBoolByType(permissionType));
      assertFalse(getPermissionCrToggleByType(arcPermissionView, permissionType)
                      .checked);

      // Toggle On.
      await clickPermissionToggle(permissionType);
      assertTrue(getPermissionBoolByType(permissionType));
      assertTrue(getPermissionCrToggleByType(arcPermissionView, permissionType)
                     .checked);
    };

    expandPermissions();
    await checkPermissionToggle('kLocation');
    await checkPermissionToggle('kCamera');
    await checkPermissionToggle('kNotifications');
  });


  test('OnClick handler for permission item works correctly', async () => {
    const checkPermissionItemOnClick = async (permissionType) => {
      assertTrue(getPermissionBoolByType(permissionType));
      assertTrue(getPermissionCrToggleByType(arcPermissionView, permissionType)
                     .checked);

      // Toggle Off.
      await clickPermissionItem(permissionType);
      assertFalse(getPermissionBoolByType(permissionType));
      assertFalse(getPermissionCrToggleByType(arcPermissionView, permissionType)
                      .checked);

      // Toggle On.
      await clickPermissionItem(permissionType);
      assertTrue(getPermissionBoolByType(permissionType));
      assertTrue(getPermissionCrToggleByType(arcPermissionView, permissionType)
                     .checked);
    };

    expandPermissions();
    await checkPermissionItemOnClick('kLocation');
    await checkPermissionItemOnClick('kCamera');
    await checkPermissionItemOnClick('kNotifications');
    await checkPermissionItemOnClick('kContacts');
    await checkPermissionItemOnClick('kStorage');
  });

  test('No permissions requested label', async () => {
    assertTrue(isHiddenByDomIf(
        arcPermissionView.root.querySelector('#no-permissions')));

    // Create an ARC app without any permissions.
    const arcOptions = {
      type: appManagement.mojom.AppType.kArc,
      permissions: FakePageHandler.createArcPermissions([])
    };

    // Add an arc app, and make it the currently selected app.
    const app = await fakeHandler.addApp(null, arcOptions);
    AppManagementStore.getInstance().dispatch(updateSelectedAppId(app.id));
    await flushTasks();

    assertFalse(isHiddenByDomIf(
        arcPermissionView.root.querySelector('#no-permissions')));
  });
});
