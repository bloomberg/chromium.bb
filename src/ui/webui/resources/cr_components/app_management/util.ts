// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';

import {App} from './app_management.mojom-webui.js';
import {AppManagementUserAction, AppType, OptionalBool} from './constants.js';
import {PermissionType, PermissionTypeIndex} from './permission_constants.js';
import {isPermissionEnabled} from './permission_util.js';

/**
 * @fileoverview Utility functions for the App Management page.
 */

type AppManagementPageState = {
  apps: Record<string, App>,
  selectedAppId: string|null,
};

export function createEmptyState(): AppManagementPageState {
  return {
    apps: {},
    selectedAppId: null,
  };
}

export function createInitialState(apps: Array<App>): AppManagementPageState {
  const initialState = createEmptyState();

  for (const app of apps) {
    initialState.apps[app.id] = app;
  }

  return initialState;
}

export function getAppIcon(app: App): string {
  return `chrome://app-icon/${app.id}/64`;
}

export function getPermissionValueBool(
    app: App, permissionType: PermissionTypeIndex): boolean {
  const permission = getPermission(app, permissionType);
  assert(permission);

  return isPermissionEnabled(permission.value);
}

/**
 * Undefined is returned when the app does not request a permission.
 */
export function getPermission(app: App, permissionType: PermissionTypeIndex) {
  return app.permissions[PermissionType[permissionType]];
}

export function getSelectedApp(state: AppManagementPageState): App|null {
  const selectedAppId = state.selectedAppId;
  return selectedAppId ? state.apps[selectedAppId] : null;
}

/**
 * A comparator function to sort strings alphabetically.
 */
export function alphabeticalSort(a: string, b: string) {
  return a.localeCompare(b);
}

/**
 * Toggles an OptionalBool
 */
export function toggleOptionalBool(bool: OptionalBool): OptionalBool {
  switch (bool) {
    case OptionalBool.kFalse:
      return OptionalBool.kTrue;
    case OptionalBool.kTrue:
      return OptionalBool.kFalse;
    default:
      assertNotReached();
      return OptionalBool.kFalse;
  }
}

export function convertOptionalBoolToBool(optionalBool: OptionalBool): boolean {
  switch (optionalBool) {
    case OptionalBool.kTrue:
      return true;
    case OptionalBool.kFalse:
      return false;
    default:
      assertNotReached();
      return false;
  }
}

export function getUserActionHistogramNameForAppType_(appType: AppType):
    string {
  switch (appType) {
    case AppType.kArc:
      return 'AppManagement.AppDetailViews.ArcApp';
    case AppType.kChromeApp:
    case AppType.kStandaloneBrowser:
    case AppType.kStandaloneBrowserChromeApp:
      // TODO(https://crbug.com/1225848): Figure out appropriate behavior for
      // Lacros-hosted chrome-apps.
      return 'AppManagement.AppDetailViews.ChromeApp';
    case AppType.kWeb:
      return 'AppManagement.AppDetailViews.WebApp';
    case AppType.kPluginVm:
      return 'AppManagement.AppDetailViews.PluginVmApp';
    case AppType.kBorealis:
      return 'AppManagement.AppDetailViews.BorealisApp';
    default:
      assertNotReached();
      return '';
  }
}

export function recordAppManagementUserAction(
    appType: AppType, userAction: AppManagementUserAction) {
  const histogram = getUserActionHistogramNameForAppType_(appType);
  const enumLength = Object.keys(AppManagementUserAction).length;
  chrome.metricsPrivate.recordEnumerationValue(
      histogram, userAction, enumLength);
}
