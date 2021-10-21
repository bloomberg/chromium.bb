// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-stylus' is the settings subpage with stylus-specific settings.
 */

const FIND_MORE_APPS_URL = 'https://play.google.com/store/apps/' +
    'collection/promotion_30023cb_stylus_apps';

import {afterNextRender, Polymer, html, flush, Templatizer, TemplateInstanceBase} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assert, assertNotReached} from '//resources/js/assert.m.js';
import '//resources/cr_elements/cr_link_row/cr_link_row.js';
import '//resources/cr_elements/cr_toggle/cr_toggle.m.js';
import '//resources/cr_elements/shared_vars_css.m.js';
import '//resources/js/action_link.js';
import '//resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import {CrPolicyIndicatorType} from '//resources/cr_elements/policy/cr_policy_indicator_behavior.m.js';
import {I18nBehavior} from '//resources/js/i18n_behavior.m.js';
import {loadTimeData} from '//resources/js/load_time_data.m.js';
import {BatteryStatus, DevicePageBrowserProxy, DevicePageBrowserProxyImpl, ExternalStorage, IdleBehavior, LidClosedBehavior, NoteAppInfo, NoteAppLockScreenSupport, PowerManagementSettings, PowerSource, getDisplayApi, StorageSpaceState} from './device_page_browser_proxy.js';
import '../../controls/settings_toggle_button.js';
import '../../settings_shared_css.js';
import {Router, Route, RouteObserverBehavior} from '../../router.js';
import {routes} from '../os_route.m.js';
import {recordSettingChange, recordSearch, setUserActionRecorderForTesting, recordPageFocus, recordPageBlur, recordClick, recordNavigation} from '../metrics_recorder.m.js';
import {DeepLinkingBehavior} from '../deep_linking_behavior.m.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'settings-stylus',

  behaviors: [
    DeepLinkingBehavior,
    RouteObserverBehavior,
  ],

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * Policy indicator type for user policy - used for policy indicator UI
     * shown when an app that is not allowed to run on lock screen by policy is
     * selected.
     * @type {CrPolicyIndicatorType}
     * @private
     */
    userPolicyIndicator_: {
      type: String,
      value: CrPolicyIndicatorType.USER_POLICY,
    },

    /**
     * Note taking apps the user can pick between.
     * @private {Array<!NoteAppInfo>}
     */
    appChoices_: {
      type: Array,
      value() {
        return [];
      }
    },

    /**
     * True if the device has an internal stylus.
     * @private
     */
    hasInternalStylus_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('hasInternalStylus');
      },
      readOnly: true,
    },

    /**
     * Currently selected note taking app.
     * @private {?NoteAppInfo}
     */
    selectedApp_: {
      type: Object,
      value: null,
    },

    /**
     * True if the ARC container has not finished starting yet.
     * @private
     */
    waitingForAndroid_: {
      type: Boolean,
      value: false,
    },

    /**
     * Used by DeepLinkingBehavior to focus this page's deep links.
     * @type {!Set<!chromeos.settings.mojom.Setting>}
     */
    supportedSettingIds: {
      type: Object,
      value: () => new Set([
        chromeos.settings.mojom.Setting.kStylusToolsInShelf,
        chromeos.settings.mojom.Setting.kStylusNoteTakingApp,
        chromeos.settings.mojom.Setting.kStylusNoteTakingFromLockScreen,
        chromeos.settings.mojom.Setting.kStylusLatestNoteOnLockScreen,
      ]),
    },
  },

  /**
   * @param {!Route} route
   * @param {Route} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    if (route !== routes.STYLUS) {
      return;
    }

    this.attemptDeepLink();
  },

  /**
   * @return {boolean} Whether note taking from the lock screen is supported
   *     by the selected note-taking app.
   * @private
   */
  supportsLockScreen_() {
    return !!this.selectedApp_ &&
        this.selectedApp_.lockScreenSupport !==
        NoteAppLockScreenSupport.NOT_SUPPORTED;
  },

  /**
   * @return {boolean} Whether the selected app is disallowed to handle note
   *     actions from lock screen as a result of a user policy.
   * @private
   */
  disallowedOnLockScreenByPolicy_() {
    return !!this.selectedApp_ &&
        this.selectedApp_.lockScreenSupport ===
        NoteAppLockScreenSupport.NOT_ALLOWED_BY_POLICY;
  },

  /**
   * @return {boolean} Whether the selected app is enabled as a note action
   *     handler on the lock screen.
   * @private
   */
  lockScreenSupportEnabled_() {
    return !!this.selectedApp_ &&
        this.selectedApp_.lockScreenSupport ===
        NoteAppLockScreenSupport.ENABLED;
  },

  /** @private {?DevicePageBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created() {
    this.browserProxy_ = DevicePageBrowserProxyImpl.getInstance();
  },

  /** @override */
  ready() {
    this.browserProxy_.setNoteTakingAppsUpdatedCallback(
        this.onNoteAppsUpdated_.bind(this));
    this.browserProxy_.requestNoteTakingApps();
  },

  /**
   * Finds note app info with the provided app id.
   * @param {!string} id
   * @return {?NoteAppInfo}
   * @private
   */
  findApp_(id) {
    return this.appChoices_.find(function(app) {
      return app.value === id;
    }) ||
        null;
  },

  /**
   * Toggles whether the selected app is enabled as a note action handler on
   * the lock screen.
   * @private
   */
  toggleLockScreenSupport_() {
    assert(this.selectedApp_);
    if (this.selectedApp_.lockScreenSupport !==
            NoteAppLockScreenSupport.ENABLED &&
        this.selectedApp_.lockScreenSupport !==
            NoteAppLockScreenSupport.SUPPORTED) {
      return;
    }

    this.browserProxy_.setPreferredNoteTakingAppEnabledOnLockScreen(
        this.selectedApp_.lockScreenSupport ===
        NoteAppLockScreenSupport.SUPPORTED);
    recordSettingChange();
  },

  /** @private */
  onSelectedAppChanged_() {
    const app = this.findApp_(this.$.selectApp.value);
    this.selectedApp_ = app;

    if (app && !app.preferred) {
      this.browserProxy_.setPreferredNoteTakingApp(app.value);
      recordSettingChange();
    }
  },

  /**
   * @param {Array<!NoteAppInfo>} apps
   * @param {boolean} waitingForAndroid
   * @private
   */
  onNoteAppsUpdated_(apps, waitingForAndroid) {
    this.waitingForAndroid_ = waitingForAndroid;
    this.appChoices_ = apps;

    // Wait until app selection UI is updated before setting the selected app.
    this.async(this.onSelectedAppChanged_.bind(this));
  },

  /**
   * @param {Array<!NoteAppInfo>} apps
   * @param {boolean} waitingForAndroid
   * @private
   */
  showNoApps_(apps, waitingForAndroid) {
    return apps.length === 0 && !waitingForAndroid;
  },

  /**
   * @param {Array<!NoteAppInfo>} apps
   * @param {boolean} waitingForAndroid
   * @private
   */
  showApps_(apps, waitingForAndroid) {
    return apps.length > 0 && !waitingForAndroid;
  },

  /** @private */
  onFindAppsTap_() {
    this.browserProxy_.showPlayStore(FIND_MORE_APPS_URL);
  },
});
