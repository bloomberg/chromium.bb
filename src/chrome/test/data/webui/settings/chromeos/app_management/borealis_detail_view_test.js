// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

import {PermissionType, createBoolPermission, AppManagementStore, updateSelectedAppId, getPermissionValueBool, convertOptionalBoolToBool, Router} from 'chrome://os-settings/chromeos/os_settings.js';
import {setupFakeHandler, replaceStore, replaceBody, getPermissionCrToggleByType, getPermissionToggleByType} from './test_util.js';
import {eventToPromise, flushTasks} from 'chrome://test/test_util.js';

suite('<app-management-borealis-detail-view>', function() {
  let borealisDetailView;
  let fakeHandler;

  const kBorealisClientAppId = 'epfhbkiklgmlkhfpbcdleadnhcfdjfmo';

  function getPermissionBoolByType(permissionType) {
    return getPermissionValueBool(borealisDetailView.app_, permissionType);
  }

  async function clickToggle(permissionType) {
    getPermissionToggleByType(borealisDetailView, permissionType).click();
    await fakeHandler.flushPipesForTesting();
  }

  function getSelectedAppFromStore() {
    const storeData = AppManagementStore.getInstance().data;
    return storeData.apps[storeData.selectedAppId];
  }

  setup(async function() {
    fakeHandler = setupFakeHandler();
    replaceStore();

    const permissions = {};
    const permissionTypes = [PermissionType.kMicrophone];
    for (const permissionType of permissionTypes) {
      permissions[permissionType] = createBoolPermission(
          permissionType, true /*permission_value*/, false /*is_managed*/);
    }

    // Add main app, and make it the currently selected app.
    const mainOptions = {
      type: appManagement.mojom.AppType.kBorealis,
      permissions: permissions
    };
    const mainApp = await fakeHandler.addApp(kBorealisClientAppId, mainOptions);
    AppManagementStore.getInstance().dispatch(updateSelectedAppId(mainApp.id));
    borealisDetailView =
        document.createElement('app-management-borealis-detail-view');
    replaceBody(borealisDetailView);
  });

  test('App is rendered correctly', function() {
    assertEquals(
        AppManagementStore.getInstance().data.selectedAppId,
        borealisDetailView.app_.id);
  });

  test('Toggle permissions', async function() {
    const checkToggle = async (permissionType) => {
      assertTrue(getPermissionBoolByType(permissionType));
      assertTrue(getPermissionCrToggleByType(borealisDetailView, permissionType)
                     .checked);

      // Toggle off.
      await clickToggle(permissionType);
      assertFalse(getPermissionBoolByType(permissionType));
      assertFalse(
          getPermissionCrToggleByType(borealisDetailView, permissionType)
              .checked);

      // Toggle on.
      await clickToggle(permissionType);
      assertTrue(getPermissionBoolByType(permissionType));
      assertTrue(getPermissionCrToggleByType(borealisDetailView, permissionType)
                     .checked);
    };

    await checkToggle('kMicrophone');
  });

  test('Pin to shelf toggle', async function() {
    const pinToShelfItem = borealisDetailView.$['pin-to-shelf-setting'];
    const toggle = pinToShelfItem.$['toggle-row'].$.toggle;

    assertFalse(toggle.checked);
    assertEquals(
        toggle.checked,
        convertOptionalBoolToBool(getSelectedAppFromStore().isPinned));
    pinToShelfItem.click();
    await fakeHandler.flushPipesForTesting();
    assertTrue(toggle.checked);
    assertEquals(
        toggle.checked,
        convertOptionalBoolToBool(getSelectedAppFromStore().isPinned));
    pinToShelfItem.click();
    await fakeHandler.flushPipesForTesting();
    assertFalse(toggle.checked);
    assertEquals(
        toggle.checked,
        convertOptionalBoolToBool(getSelectedAppFromStore().isPinned));
  });

  test('Permission info links are correct', async function() {
    assertTrue(!!borealisDetailView.$$('#main-link'));
    assertFalse(!!borealisDetailView.$$('#borealis-link'));

    // Add borealis (non main) app. Note that any tests after this will
    // have the borealis app selected as default.
    const options = {
      type: appManagement.mojom.AppType.kBorealis,
    };
    const app = await fakeHandler.addApp('foo', options);
    AppManagementStore.getInstance().dispatch(updateSelectedAppId(app.id));
    await fakeHandler.flushPipesForTesting();
    assertFalse(!!borealisDetailView.$$('#main-link'));
    assertTrue(!!borealisDetailView.$$('#borealis-link'));

    // Check that link directs to main app page.
    const link = borealisDetailView.$$('#borealis-link');
    const anchorTag = link.shadowRoot.querySelector('a');
    assertTrue(!!anchorTag);
    const localizedLinkPromise = eventToPromise('link-clicked', link);
    anchorTag.click();
    await Promise.all([localizedLinkPromise, flushTasks()]);
    await fakeHandler.flushPipesForTesting();
    assertEquals(
        Router.getInstance().getQueryParameters().get('id'),
        kBorealisClientAppId);
  });
});
