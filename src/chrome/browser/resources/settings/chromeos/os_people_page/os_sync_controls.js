// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_toggle/cr_toggle.m.js';
import '//resources/cr_elements/shared_vars_css.m.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../settings_shared_css.js';
import '//resources/cr_components/chromeos/localized_link/localized_link.js';

import {assert, assertNotReached} from '//resources/js/assert.m.js';
import {I18nBehavior} from '//resources/js/i18n_behavior.m.js';
import {loadTimeData} from '//resources/js/load_time_data.m.js';
import {WebUIListenerBehavior} from '//resources/js/web_ui_listener_behavior.m.js';
import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {StatusAction, SyncBrowserProxyImpl} from '../../people_page/sync_browser_proxy.js';
import {Route, Router} from '../../router.js';
import {DeepLinkingBehavior} from '../deep_linking_behavior.m.js';
import {recordClick, recordNavigation, recordPageBlur, recordPageFocus, recordSearch, recordSettingChange, setUserActionRecorderForTesting} from '../metrics_recorder.m.js';
import {routes} from '../os_route.m.js';
import {RouteObserverBehavior} from '../route_observer_behavior.js';

import {OsSyncBrowserProxy, OsSyncBrowserProxyImpl, OsSyncPrefs} from './os_sync_browser_proxy.m.js';


/**
 * Names of the individual data type properties to be cached from
 * OsSyncPrefs when the user checks 'Sync All'.
 * @type {!Array<string>}
 */
const SyncPrefsIndividualDataTypes = [
  'osAppsSynced',
  'osPreferencesSynced',
  'osWifiConfigurationsSynced',

  // Note: Wallpaper uses a different naming scheme because it's stored as its
  // own separate pref instead of through the sync service.
  'wallpaperEnabled',
];

/**
 * @fileoverview
 * 'os-sync-controls' contains all OS sync data type controls.
 */
Polymer({
  _template: html`{__html_template__}`,
  is: 'os-sync-controls',

  behaviors: [
    DeepLinkingBehavior,
    I18nBehavior,
    RouteObserverBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    hidden: {
      type: Boolean,
      value: true,
      computed: 'syncControlsHidden_(osSyncPrefs)',
      reflectToAttribute: true,
    },

    /**
     * Injected sync system status. Undefined until the parent component injects
     * the value.
     * @type {SyncStatus|undefined}
     */
    syncStatus: Object,

    /**
     * Injected profile icon URL, usually a data:image/png URL.
     * @private
     */
    profileIconUrl: String,

    /**
     * Injected profile name, e.g. "John Cena".
     * @private
     */
    profileName: String,

    /**
     * Injected profile email address, e.g. "john.cena@gmail.com".
     * @private
     */
    profileEmail: String,

    /**
     * The current OS sync preferences. Cached so we can restore individual
     * toggle state when turning "sync everything" on and off, without affecting
     * the underlying chrome prefs.
     * @type {OsSyncPrefs|undefined}
     */
    osSyncPrefs: Object,

    /** @private */
    areDataTypeTogglesDisabled_: {
      type: Boolean,
      value: true,
      computed: `computeDataTypeTogglesDisabled_(osSyncPrefs.syncAllOsTypes)`,
    },

    /**
     * Used by DeepLinkingBehavior to focus this page's deep links.
     * @type {!Set<!chromeos.settings.mojom.Setting>}
     */
    supportedSettingIds: {
      type: Object,
      value: () => new Set([chromeos.settings.mojom.Setting.kSplitSyncOnOff]),
    },
  },

  /** @private {?OsSyncBrowserProxy} */
  browserProxy_: null,

  /**
   * Caches the individually selected synced data types. This is used to
   * be able to restore the selections after checking and unchecking Sync All.
   * @private {?Object}
   */
  cachedOsSyncPrefs_: null,

  /** @override */
  created() {
    this.browserProxy_ = OsSyncBrowserProxyImpl.getInstance();
  },

  /** @override */
  attached() {
    this.addWebUIListener(
        'os-sync-prefs-changed', this.handleOsSyncPrefsChanged_.bind(this));
  },

  /**
   * RouteObserverBehavior
   * @param {!Route|undefined} newRoute
   * @param {!Route|undefined} oldRoute
   * @protected
   */
  currentRouteChanged(newRoute, oldRoute) {
    if (newRoute === routes.OS_SYNC) {
      this.browserProxy_.didNavigateToOsSyncPage();
      this.attemptDeepLink();
    }
    if (oldRoute === routes.OS_SYNC) {
      this.browserProxy_.didNavigateAwayFromOsSyncPage();
    }
  },

  /**
   * @return {string} The top label for the account row.
   * @private
   */
  getAccountTitle_() {
    if (!this.syncStatus) {
      return '';
    }
    return this.syncStatus.hasError ? this.i18n('syncNotWorking') :
                                      this.profileName;
  },

  /**
   * @return {string} The bottom label for the account row.
   * @private
   */
  getAccountSubtitle_() {
    if (!this.syncStatus) {
      return '';
    }
    return !this.syncStatus.hasError ?
        this.i18n('syncingTo', this.profileEmail) :
        this.profileEmail;
  },

  /**
   * Returns the CSS class for the sync status icon.
   * @return {string}
   * @private
   */
  getSyncIconStyle_() {
    if (!this.syncStatus) {
      return 'sync';
    }
    if (this.syncStatus.disabled) {
      return 'sync-disabled';
    }
    if (!this.syncStatus.hasError) {
      return 'sync';
    }
    // Specific error cases below.
    if (this.syncStatus.hasUnrecoverableError) {
      return 'sync-problem';
    }
    if (this.syncStatus.statusAction === StatusAction.REAUTHENTICATE) {
      return 'sync-paused';
    }
    return 'sync-problem';
  },

  /**
   * Returns the image to use for the sync status icon. The value must match
   * one of iron-icon's settings:(*) icon names.
   * @return {string}
   * @private
   */
  getSyncIcon_() {
    switch (this.getSyncIconStyle_()) {
      case 'sync-problem':
        return 'settings:sync-problem';
      case 'sync-paused':
        return 'settings:sync-disabled';
      default:
        return 'cr:sync';
    }
  },

  /**
   * Handler for when the sync preferences are updated.
   * @private
   */
  handleOsSyncPrefsChanged_(osSyncPrefs) {
    this.osSyncPrefs = osSyncPrefs;

    // If apps are not registered or synced, force wallpaper off.
    if (!this.osSyncPrefs.osAppsRegistered || !this.osSyncPrefs.osAppsSynced) {
      this.set('osSyncPrefs.wallpaperEnabled', false);
    }
  },

  /**
   * Handler for when the sync all data types checkbox is changed.
   * @param {!Event} event
   * @private
   */
  onSyncAllOsTypesChanged_(event) {
    if (event.target.checked) {
      this.set('osSyncPrefs.syncAllOsTypes', true);

      // Cache the previously selected preference before checking every box.
      this.cachedOsSyncPrefs_ = {};
      for (const dataType of SyncPrefsIndividualDataTypes) {
        // These are all booleans, so this shallow copy is sufficient.
        this.cachedOsSyncPrefs_[dataType] = this.osSyncPrefs[dataType];

        this.set(['osSyncPrefs', dataType], true);
      }
    } else if (this.cachedOsSyncPrefs_) {
      // Restore the previously selected preference.
      for (const dataType of SyncPrefsIndividualDataTypes) {
        this.set(['osSyncPrefs', dataType], this.cachedOsSyncPrefs_[dataType]);
      }
    }

    this.sendOsSyncDatatypes_();
  },

  /**
   * Handler for when any sync data type checkbox is changed.
   * @private
   */
  onSingleSyncDataTypeChanged_() {
    this.sendOsSyncDatatypes_();
  },

  /**
   * Handler for changes to the apps sync state; apps have a special handler
   * instead of relying on onSingleSyncDataTypeChanged_() because wallpaper has
   * a dependency on apps.
   * @private
   */
  onAppsSyncedChanged_() {
    this.set('osSyncPrefs.wallpaperEnabled', this.osSyncPrefs.osAppsSynced);

    this.onSingleSyncDataTypeChanged_();
  },

  /**
   * Sends the osSyncPrefs dictionary back to the C++ handler.
   * @private
   */
  sendOsSyncDatatypes_() {
    assert(this.osSyncPrefs);
    this.browserProxy_.setOsSyncDatatypes(this.osSyncPrefs);
  },

  /**
   * @return {boolean} Whether the sync data type toggles should be disabled.
   * @private
   */
  computeDataTypeTogglesDisabled_() {
    return this.osSyncPrefs !== undefined && this.osSyncPrefs.syncAllOsTypes;
  },

  /**
   * @return {boolean} Whether the sync controls are hidden.
   * @private
   */
  syncControlsHidden_() {
    // Hide everything until the initial prefs are received from C++,
    // otherwise there is a visible layout reshuffle on first load.
    return !this.osSyncPrefs;
  },

  /**
   * @return {boolean} Whether the wallpaper checkbox and label should be
   *     disabled.
   * @private
   */
  shouldWallpaperSyncSectionBeDisabled_() {
    return this.areDataTypeTogglesDisabled_ || !this.osSyncPrefs ||
        !this.osSyncPrefs.osAppsSynced;
  },
});
