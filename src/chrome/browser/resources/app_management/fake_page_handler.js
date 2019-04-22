// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('app_management', function() {
  /**
   * @implements {appManagement.mojom.PageHandlerInterface}
   */
  class FakePageHandler {
    /**
     * @return {!Object<number, Permission>}
     */
    static createWebPermissions() {
      const permissionIds = [
        PwaPermissionType.CONTENT_SETTINGS_TYPE_GEOLOCATION,
        PwaPermissionType.CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
        PwaPermissionType.CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC,
        PwaPermissionType.CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA,
      ];

      const permissions = {};

      for (const permissionId of permissionIds) {
        permissions[permissionId] = app_management.util.createPermission(
            permissionId, PermissionValueType.kTriState, TriState.kAllow);
      }

      return permissions;
    }

    /**
     * @param {Array<number>=} optIds
     * @return {!Object<number, Permission>}
     */
    static createArcPermissions(optIds) {
      const permissionIds = optIds || [
        ArcPermissionType.CAMERA,
        ArcPermissionType.LOCATION,
        ArcPermissionType.MICROPHONE,
        ArcPermissionType.NOTIFICATIONS,
      ];

      const permissions = {};

      for (const permissionId of permissionIds) {
        permissions[permissionId] = app_management.util.createPermission(
            permissionId, PermissionValueType.kBool, Bool.kTrue);
      }

      return permissions;
    }

    /**
     * @param {AppType} appType
     * @return {!Object<number, Permission>}
     */
    static createPermissions(appType) {
      switch (appType) {
        case (AppType.kWeb):
          return FakePageHandler.createWebPermissions();
        case (AppType.kArc):
          return FakePageHandler.createArcPermissions();
        default:
          return {};
      }
    }

    /**
     * @param {string} id
     * @param {Object=} optConfig
     * @return {!App}
     */
    static createApp(id, optConfig) {
      const app = {
        id: id,
        type: apps.mojom.AppType.kWeb,
        title: 'App Title',
        description: '',
        version: '5.1',
        size: '9.0MB',
        isPinned: apps.mojom.OptionalBool.kFalse,
        permissions: {},
      };

      if (optConfig) {
        Object.assign(app, optConfig);
      }

      // Only create default permissions if none were provided in the config.
      if (!optConfig || optConfig.permissions === undefined) {
        app.permissions = FakePageHandler.createPermissions(app.type);
      }

      return app;
    }

    /**
     * @param {appManagement.mojom.PageProxy} page
     */
    constructor(page) {
      /** @type {appManagement.mojom.PageProxy} */
      this.page = page;

      /** @type {!Array<App>} */
      this.apps_ = [];

      this.$ = {
        flushForTesting: async () => {
          await this.page.$.flushForTesting();
        }
      };

      /** @type {number} */
      this.guid = 0;
    }

    async getApps() {
      return {apps: this.apps_};
    }

    /**
     * @param {string} appId
     * @return {!Promise}
     */
    async getExtensionAppPermissionMessages(appId) {
      return [];
    }

    /**
     * @param {!Array<App>} appList
     */
    setApps(appList) {
      this.apps_ = appList;
    }

    /**
     * @param {string} appId
     * @param {OptionalBool} pinnedValue
     */
    setPinned(appId, pinnedValue) {
      const app = app_management.Store.getInstance().data.apps[appId];

      const newApp =
          /** @type {App} */ (Object.assign({}, app, {isPinned: pinnedValue}));
      this.page.onAppChanged(newApp);
    }

    /**
     * @param {string} appId
     * @param {Permission} permission
     */
    setPermission(appId, permission) {
      const app = app_management.Store.getInstance().data.apps[appId];

      // Check that the app had a previous value for the given permission
      assert(app.permissions[permission.permissionId]);

      const newPermissions = Object.assign({}, app.permissions);
      newPermissions[permission.permissionId] = permission;
      const newApp = /** @type {App} */ (
          Object.assign({}, app, {permissions: newPermissions}));
      this.page.onAppChanged(newApp);
    }

    /**
     * @param {string} appId
     */
    uninstall(appId) {
      this.page.onAppRemoved(appId);
    }

    /**
     * @param {string} appId
     */
    openNativeSettings(appId) {}

    /**
     * @param {string} optId
     * @param {Object=} optConfig
     * @return {!Promise<!App>}
     */
    async addApp(optId, optConfig) {
      optId = optId || String(this.guid++);
      const app = FakePageHandler.createApp(optId, optConfig);
      this.page.onAppAdded(app);
      await this.$.flushForTesting();
      return app;
    }

    /**
     * Takes an app id and an object mapping app fields to the values they
     * should be changed to, and dispatches an action to carry out these
     * changes.
     * @param {string} id
     * @param {Object} changes
     */
    async changeApp(id, changes) {
      this.page.onAppChanged(FakePageHandler.createApp(id, changes));
      await this.$.flushForTesting();
    }
  }

  return {FakePageHandler: FakePageHandler};
});
