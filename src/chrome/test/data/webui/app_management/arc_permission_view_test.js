// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

suite('<app-management-arc-permission-view>', () => {
  let arcPermissionView;
  let fakeHandler;

  function getPermissionItemByPermissionType(permissionType) {
    return arcPermissionView.root.querySelector(
        '[permission-type=' + permissionType + ']');
  }

  function expandPermissions() {
    arcPermissionView.root.querySelector('#subpermission-expand-row').click();
  }

  function getPermissionBoolByType(permissionType) {
    return app_management.util.getPermissionValueBool(
        arcPermissionView.app_, permissionType);
  }

  function getPermissionToggleByType(permissionType) {
    return arcPermissionView.root
        .querySelector('[permission-type=' + permissionType + ']')
        .root.querySelector('app-management-permission-toggle')
        .root.querySelector('cr-toggle');
  }

  async function clickPermissionToggle(permissionType) {
    getPermissionToggleByType(permissionType).click();
    await fakeHandler.$.flushForTesting();
  }

  async function clickPermissionItem(permissionType) {
    getPermissionItemByPermissionType(permissionType).click();
    await fakeHandler.$.flushForTesting();
  }

  setup(async () => {
    fakeHandler = setupFakeHandler();
    replaceStore();

    // Create an ARC app without microphone permissions.
    const arcOptions = {
      type: apps.mojom.AppType.kArc,
      permissions: app_management.FakePageHandler.createArcPermissions([
        ArcPermissionType.CAMERA,
        ArcPermissionType.LOCATION,
        ArcPermissionType.NOTIFICATIONS,
      ])
    };

    // Add an arc app, and make it the currently selected app.
    const app = await fakeHandler.addApp(null, arcOptions);
    app_management.Store.getInstance().dispatch(
        app_management.actions.changePage(PageType.DETAIL, app.id));

    arcPermissionView =
        document.createElement('app-management-arc-permission-view');
    replaceBody(arcPermissionView);
  });

  test('App is rendered correctly', () => {
    assertEquals(
        app_management.Store.getInstance().data.currentPage.selectedAppId,
        arcPermissionView.app_.id);
  });

  test('Permissions are hidden correctly', () => {
    expandPermissions();
    assertTrue(isHidden(getPermissionItemByPermissionType('MICROPHONE')));
    assertFalse(isHidden(getPermissionItemByPermissionType('LOCATION')));
    assertFalse(isHidden(getPermissionItemByPermissionType('CAMERA')));
  });

  test('Toggle works correctly', async () => {
    const checkPermissionToggle = async (permissionType) => {
      assertTrue(getPermissionBoolByType(permissionType));
      assertTrue(getPermissionToggleByType(permissionType).checked);

      // Toggle Off.
      await clickPermissionToggle(permissionType);
      assertFalse(getPermissionBoolByType(permissionType));
      assertFalse(getPermissionToggleByType(permissionType).checked);

      // Toggle On.
      await clickPermissionToggle(permissionType);
      assertTrue(getPermissionBoolByType(permissionType));
      assertTrue(getPermissionToggleByType(permissionType).checked);
    };

    expandPermissions();
    await checkPermissionToggle('LOCATION');
    await checkPermissionToggle('CAMERA');
    await checkPermissionToggle('NOTIFICATIONS');
  });


  test('OnClick handler for permission item works correctly', async () => {
    const checkPermissionItemOnClick = async (permissionType) => {
      assertTrue(getPermissionBoolByType(permissionType));
      assertTrue(getPermissionToggleByType(permissionType).checked);

      // Toggle Off.
      await clickPermissionItem(permissionType);
      assertFalse(getPermissionBoolByType(permissionType));
      assertFalse(getPermissionToggleByType(permissionType).checked);

      // Toggle On.
      await clickPermissionItem(permissionType);
      assertTrue(getPermissionBoolByType(permissionType));
      assertTrue(getPermissionToggleByType(permissionType).checked);
    };

    expandPermissions();
    await checkPermissionItemOnClick('LOCATION');
    await checkPermissionItemOnClick('CAMERA');
    await checkPermissionItemOnClick('NOTIFICATIONS');
  });
});
