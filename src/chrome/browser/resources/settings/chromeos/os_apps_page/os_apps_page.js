// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-apps-page' is the settings page containing app related settings.
 *
 */
import '//resources/cr_elements/cr_button/cr_button.m.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import '//resources/cr_elements/cr_link_row/cr_link_row.js';
import '//resources/cr_elements/policy/cr_policy_pref_indicator.m.js';
import '../../settings_page/settings_animated_pages.js';
import '../../settings_page/settings_subpage.js';
import '../../settings_shared_css.js';
import '../../settings_shared_css.js';
import '../guest_os/guest_os_shared_usb_devices.js';
import '../guest_os/guest_os_shared_paths.js';
import '//resources/cr_components/chromeos/localized_link/localized_link.js';
import './android_apps_subpage.js';
import './app_notifications_page/app_notifications_subpage.js';
import './app_management_page/app_management_page.js';
import './app_management_page/app_detail_view.js';
import './app_management_page/uninstall_button.js';
import '../../controls/settings_dropdown_menu.js';

import {I18nBehavior} from '//resources/js/i18n_behavior.m.js';
import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';

import {loadTimeData} from '../../i18n_setup.js';
import {Route, Router} from '../../router.js';
import {DeepLinkingBehavior} from '../deep_linking_behavior.m.js';
import {routes} from '../os_route.m.js';
import {PrefsBehavior} from '../prefs_behavior.js';
import {RouteObserverBehavior} from '../route_observer_behavior.js';

import {AndroidAppsBrowserProxyImpl, AndroidAppsInfo} from './android_apps_browser_proxy.js';
import {AppManagementEntryPoint, AppManagementEntryPointsHistogramName} from './app_management_page/constants.js';
import {AppManagementStoreClient} from './app_management_page/store_client.js';
import {getAppIcon, getSelectedApp} from './app_management_page/util.js';
import {getAppNotificationProvider} from './app_notifications_page/mojo_interface_provider.js';

/**
 * @param {!chromeos.settings.appNotification.mojom.App} app
 * @return {boolean}
 */
export function isAppInstalled(app) {
  switch (app.readiness) {
    case apps.mojom.Readiness.kReady:
    case apps.mojom.Readiness.kDisabledByBlocklist:
    case apps.mojom.Readiness.kDisabledByPolicy:
    case apps.mojom.Readiness.kDisabledByUser:
    case apps.mojom.Readiness.kTerminated:
      return true;
    case apps.mojom.Readiness.kUninstalledByUser:
    case apps.mojom.Readiness.kUninstalledByMigration:
    case apps.mojom.Readiness.kRemoved:
    case apps.mojom.Readiness.kUnknown:
      return false;
  }
  assertNotReached();
  return false;
}

Polymer({
  _template: html`{__html_template__}`,
  is: 'os-settings-apps-page',

  behaviors: [
    AppManagementStoreClient,
    DeepLinkingBehavior,
    I18nBehavior,
    PrefsBehavior,
    RouteObserverBehavior,
  ],

  /**
   * Mojo interface provider for AppNotifications.
   * @private
   */
  mojoInterfaceProvider_: null,

  /**
   * Receiver responsible for observing app notification events.
   * @private {
   *    ?chromeos.settings.appNotification.mojom.
   *    AppNotificationsObserverReceiver
   * }
   */
  appNotificationsObserverReceiver_: null,

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * This object holds the playStoreEnabled and settingsAppAvailable boolean.
     * @type {Object}
     */
    androidAppsInfo: Object,

    /**
     * If the Play Store app is available.
     * @type {boolean}
     */
    havePlayStoreApp: Boolean,

    /**
     * @type {string}
     */
    searchTerm: String,

    /**
     * Show ARC++ related settings and sub-page.
     * @type {boolean}
     */
    showAndroidApps: Boolean,

    /**
     * Whether the App Notifications page should be shown.
     * @type {boolean}
     */
    showAppNotificationsRow_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('showOsSettingsAppNotificationsRow');
      },
    },

    /**
     * Show Plugin VM shared folders sub-page.
     * @type {boolean}
     */
    showPluginVm: Boolean,

    /**
     * Show On startup settings and sub-page.
     * @type {boolean}
     */
    showStartup: Boolean,

    /** @private {!Map<string, string>} */
    focusConfig_: {
      type: Object,
      value() {
        const map = new Map();
        if (routes.APP_MANAGEMENT) {
          map.set(routes.APP_MANAGEMENT.path, '#appManagement');
        }
        if (routes.ANDROID_APPS_DETAILS) {
          map.set(
              routes.ANDROID_APPS_DETAILS.path, '#android-apps .subpage-arrow');
        }
        return map;
      },
    },

    /**
     * @type {App}
     * @private
     */
    app_: Object,

    /**
     * @type {!Array<!Object>}
     * @private
     */
    appsWithNotifications_: {
      type: Array,
      value: [],
    },

    /**
     * List of options for the on startup drop-down menu.
     * @type {!DropdownMenuOptionList}
     */
    onStartupOptions_: {
      readOnly: true,
      type: Array,
      value() {
        return [
          {value: 1, name: loadTimeData.getString('onStartupAlways')},
          {value: 2, name: loadTimeData.getString('onStartupAskEveryTime')},
          {value: 3, name: loadTimeData.getString('onStartupDoNotRestore')},
        ];
      },
    },

    /**
     * Used by DeepLinkingBehavior to focus this page's deep links.
     * @type {!Set<!chromeos.settings.mojom.Setting>}
     */
    supportedSettingIds: {
      type: Object,
      value: () => new Set([
        chromeos.settings.mojom.Setting.kManageAndroidPreferences,
        chromeos.settings.mojom.Setting.kTurnOnPlayStore,
        chromeos.settings.mojom.Setting.kRestoreAppsAndPages,
      ]),
    },
  },

  attached() {
    this.watch('app_', state => getSelectedApp(state));

    this.mojoInterfaceProvider_ = getAppNotificationProvider();

    this.appNotificationsObserverReceiver_ =
        new chromeos.settings.appNotification.mojom
            .AppNotificationsObserverReceiver(
                /**
                 * @type {!chromeos.settings.appNotification.mojom.
                 * AppNotificationsObserverInterface}
                 */
                (this));

    this.mojoInterfaceProvider_.addObserver(
        this.appNotificationsObserverReceiver_.$.bindNewPipeAndPassRemote());

    this.mojoInterfaceProvider_.getApps().then((result) => {
      this.appsWithNotifications_ = result.apps;
    });
  },

  /**
   * @param {!Route} route
   * @param {!Route} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    if (route !== routes.APPS) {
      return;
    }

    this.attemptDeepLink();
  },

  /**
   * @param {App} app
   * @return {string}
   * @private
   */
  iconUrlFromId_(app) {
    if (!app) {
      return '';
    }
    return getAppIcon(app);
  },

  /** @private */
  onClickAppManagement_() {
    chrome.metricsPrivate.recordEnumerationValue(
        AppManagementEntryPointsHistogramName,
        AppManagementEntryPoint.OsSettingsMainPage,
        Object.keys(AppManagementEntryPoint).length);
    Router.getInstance().navigateTo(routes.APP_MANAGEMENT);
  },

  /** @private */
  onClickAppNotifications_() {
    Router.getInstance().navigateTo(routes.APP_NOTIFICATIONS);
  },

  /**
   * @param {!Event} event
   * @private
   */
  onEnableAndroidAppsTap_(event) {
    this.setPrefValue('arc.enabled', true);
    event.stopPropagation();
  },

  /**
   * @return {boolean}
   * @private
   */
  isEnforced_(pref) {
    return pref.enforcement === chrome.settingsPrivate.Enforcement.ENFORCED;
  },

  /** @private */
  onAndroidAppsSubpageTap_(event) {
    if (this.androidAppsInfo.playStoreEnabled) {
      Router.getInstance().navigateTo(routes.ANDROID_APPS_DETAILS);
    }
  },

  /**
   * @param {!MouseEvent} event
   * @private
   */
  onManageAndroidAppsTap_(event) {
    // |event.detail| is the click count. Keyboard events will have 0 clicks.
    const isKeyboardAction = event.detail === 0;
    AndroidAppsBrowserProxyImpl.getInstance().showAndroidAppsSettings(
        isKeyboardAction);
  },

  /** Override chromeos.settings.appNotification.onNotificationAppChanged */
  onNotificationAppChanged(updatedApp) {
    const foundIdx = this.appsWithNotifications_.findIndex(app => {
      return app.id === updatedApp.id;
    });
    if (isAppInstalled(updatedApp)) {
      if (foundIdx !== -1) {
        this.splice('appsWithNotifications_', foundIdx, updatedApp);
        return;
      }
      this.push('appsWithNotifications_', updatedApp);
      return;
    }

    // Cannot have an app that is uninstalled prior to being installed.
    assert(foundIdx !== -1);
    // Uninstalled app found, remove it from the list.
    this.splice('appsWithNotifications_', foundIdx, 1);
  },

  /** Override chromeos.settings.appNotification.onQuietModeChanged */
  onQuietModeChanged(enabled) {},

  /**
   * @return {string}
   * @protected
   */
  getAppListCountDescription_() {
    return this.i18n(
        'appNotificationsCountDescription', this.appsWithNotifications_.length);
  }
});
