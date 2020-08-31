// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-clear-browsing-data-dialog' allows the user to
 * delete browsing data that has been cached by Chromium.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/cr_tabs/cr_tabs.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-pages/iron-pages.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import './history_deletion_dialog.js';
import './installed_app_checkbox.js';
import '../controls/settings_checkbox.js';
import '../icons.m.js';
import '../settings_shared_css.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DropdownMenuOptionList} from '../controls/settings_dropdown_menu.m.js';
import {loadTimeData} from '../i18n_setup.js';
import {StatusAction, SyncBrowserProxy, SyncBrowserProxyImpl, SyncStatus} from '../people_page/sync_browser_proxy.m.js';
import {routes} from '../route.js';
import {Route, RouteObserverBehavior, Router} from '../router.m.js';

import {ClearBrowsingDataBrowserProxy, ClearBrowsingDataBrowserProxyImpl, InstalledApp} from './clear_browsing_data_browser_proxy.js';

/**
 * @param {!Object} oldDialog the dialog to close
 * @param {!Object} newDialog the dialog to open
 * @private
 */
function replaceDialog(oldDialog, newDialog) {
  oldDialog.addEventListener('close', e => {
    e.stopPropagation();
  }, {once: true});
  oldDialog.close();
  if (!newDialog.open) {
    newDialog.showModal();
  }
}

Polymer({
  is: 'settings-clear-browsing-data-dialog',

  _template: html`{__html_template__}`,

  behaviors: [
    WebUIListenerBehavior,
    RouteObserverBehavior,
  ],

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * The current sync status, supplied by SyncBrowserProxy.
     * TODO(dpapad): make |syncStatus| private.
     * @type {?SyncStatus}
     */
    syncStatus: Object,

    /**
     * Results of browsing data counters, keyed by the suffix of
     * the corresponding data type deletion preference, as reported
     * by the C++ side.
     * @private {!Object<string>}
     */
    counters_: {
      type: Object,
      // Will be filled as results are reported.
      value() {
        return {};
      }
    },

    /**
     * List of options for the dropdown menu.
     * @private {!DropdownMenuOptionList}
     */
    clearFromOptions_: {
      readOnly: true,
      type: Array,
      value: [
        {value: 0, name: loadTimeData.getString('clearPeriodHour')},
        {value: 1, name: loadTimeData.getString('clearPeriod24Hours')},
        {value: 2, name: loadTimeData.getString('clearPeriod7Days')},
        {value: 3, name: loadTimeData.getString('clearPeriod4Weeks')},
        {value: 4, name: loadTimeData.getString('clearPeriodEverything')},
      ],
    },

    /** @private */
    clearingInProgress_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    clearButtonDisabled_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    isSupervised_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('isSupervised');
      },
    },

    /** @private */
    showHistoryDeletionDialog_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    isSignedIn_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    isSyncingHistory_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    shouldShowCookieException_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    isSyncPaused_: {
      type: Boolean,
      value: false,
      computed: 'computeIsSyncPaused_(syncStatus)',
    },

    /** @private */
    hasPassphraseError_: {
      type: Boolean,
      value: false,
      computed: 'computeHasPassphraseError_(syncStatus)',
    },

    /** @private */
    hasOtherSyncError_: {
      type: Boolean,
      value: false,
      computed:
          'computeHasOtherError_(syncStatus, isSyncPaused_, hasPassphraseError_)',
    },

    /**
     * Time in ms, when the dialog was opened.
     * @private
     */
    dialogOpenedTime_: {
      type: Number,
      value: 0,
    },

    /** @private {Array<string>} */
    tabsNames_: {
      type: Array,
      value: () =>
          [loadTimeData.getString('basicPageTitle'),
           loadTimeData.getString('advancedPageTitle'),
],
    },

    /**
     * Installed apps that might be cleared if the user clears browsing data
     * for the selected time period.
     * @private {!Array<!InstalledApp>}
     */
    installedApps_: {
      type: Array,
      value: () => [],
    },

    /** @private */
    installedAppsFlagEnabled_: {
      type: Boolean,
      value: () => loadTimeData.getBoolean('installedAppsInCbd'),
    },
  },

  listeners: {'settings-boolean-control-change': 'updateClearButtonState_'},

  /** @private {ClearBrowsingDataBrowserProxy} */
  browserProxy_: null,

  /** @private {?SyncBrowserProxy} */
  syncBrowserProxy_: null,

  /** @override */
  ready() {
    this.syncBrowserProxy_ = SyncBrowserProxyImpl.getInstance();
    this.syncBrowserProxy_.getSyncStatus().then(
        this.handleSyncStatus_.bind(this));
    this.addWebUIListener(
        'sync-status-changed', this.handleSyncStatus_.bind(this));

    this.addWebUIListener(
        'update-sync-state', this.updateSyncState_.bind(this));
    this.addWebUIListener(
        'update-counter-text', this.updateCounterText_.bind(this));
  },

  /** @override */
  attached() {
    this.browserProxy_ = ClearBrowsingDataBrowserProxyImpl.getInstance();
    this.dialogOpenedTime_ = Date.now();
    this.browserProxy_.initialize().then(() => {
      this.$.clearBrowsingDataDialog.showModal();
    });
  },

  /**
   * Handler for when the sync state is pushed from the browser.
   * @param {?SyncStatus} syncStatus
   * @private
   */
  handleSyncStatus_(syncStatus) {
    this.syncStatus = syncStatus;
  },

  /**
   * Returns true if either clearing is in progress or no data type is selected.
   * @param {boolean} clearingInProgress
   * @param {boolean} clearButtonDisabled
   * @return {boolean}
   * @private
   */
  isClearButtonDisabled_(clearingInProgress, clearButtonDisabled) {
    return clearingInProgress || clearButtonDisabled;
  },

  /**
   * Disables the Clear Data button if no data type is selected.
   * @private
   */
  updateClearButtonState_() {
    // on-select-item-changed gets called with undefined during a tab change.
    // https://github.com/PolymerElements/iron-selector/issues/95
    const tab = this.$.tabs.selectedItem;
    if (!tab) {
      return;
    }
    this.clearButtonDisabled_ = this.getSelectedDataTypes_(tab).length == 0;
  },

  /**
   * Record visits to the CBD dialog.
   *
   * RouteObserverBehavior
   * @param {!Route} currentRoute
   * @protected
   */
  currentRouteChanged(currentRoute) {
    if (currentRoute == routes.CLEAR_BROWSER_DATA) {
      chrome.metricsPrivate.recordUserAction('ClearBrowsingData_DialogCreated');
      this.dialogOpenedTime_ = Date.now();
    }
  },

  /**
   * Updates the history description to show the relevant information
   * depending on sync and signin state.
   *
   * @param {boolean} signedIn Whether the user is signed in.
   * @param {boolean} syncing Whether the user is syncing history.
   * @param {boolean} shouldShowCookieException Whether the exception about not
   *    being signed out of your Google account should be shown.
   * @private
   */
  updateSyncState_(signedIn, syncing, shouldShowCookieException) {
    this.isSignedIn_ = signedIn;
    this.isSyncingHistory_ = syncing;
    this.shouldShowCookieException_ = shouldShowCookieException;
    this.$.clearBrowsingDataDialog.classList.add('fully-rendered');
  },

  /**
   * Choose a label for the history checkbox.
   * @param {boolean} isSignedIn
   * @param {boolean} isSyncingHistory
   * @param {string} historySummary
   * @param {string} historySummarySignedIn
   * @param {string} historySummarySynced
   * @return {string}
   * @private
   */
  browsingCheckboxLabel_(
      isSignedIn, isSyncingHistory, hasSyncError, historySummary,
      historySummarySignedIn, historySummarySynced) {
    if (isSyncingHistory && !hasSyncError) {
      return historySummarySynced;
    } else if (isSignedIn && !this.isSyncPaused_) {
      return historySummarySignedIn;
    }
    return historySummary;
  },

  /**
   * Choose a label for the cookie checkbox.
   * @param {boolean} shouldShowCookieException
   * @param {string} cookiesSummary
   * @param {string} cookiesSummarySignedIn
   * @return {string}
   * @private
   */
  cookiesCheckboxLabel_(
      shouldShowCookieException, cookiesSummary, cookiesSummarySignedIn) {
    if (shouldShowCookieException) {
      return cookiesSummarySignedIn;
    }
    return cookiesSummary;
  },

  /**
   * Updates the text of a browsing data counter corresponding to the given
   * preference.
   * @param {string} prefName Browsing data type deletion preference.
   * @param {string} text The text with which to update the counter
   * @private
   */
  updateCounterText_(prefName, text) {
    // Data type deletion preferences are named "browser.clear_data.<datatype>".
    // Strip the common prefix, i.e. use only "<datatype>".
    const matches = prefName.match(/^browser\.clear_data\.(\w+)$/);
    this.set('counters_.' + assert(matches[1]), text);
  },

  /**
   * Returns a list of selected data types.
   * @param {!HTMLElement} tab
   * @return {!Array<string>}
   * @private
   */
  getSelectedDataTypes_(tab) {
    const checkboxes = tab.querySelectorAll('settings-checkbox');
    const dataTypes = [];
    checkboxes.forEach((checkbox) => {
      if (checkbox.checked && !checkbox.hidden) {
        dataTypes.push(checkbox.pref.key);
      }
    });
    return dataTypes;
  },

  /**
   * Gets a list of top 5 installed apps that have been launched
   * within the time period selected. This is used to warn the user
   * that data for these apps will be cleared as well, and offers
   * them the option to exclude deletion of this data.
   * @return {!Promise}
   * @private
   */
  getInstalledApps_: async function() {
    const tab = this.$.tabs.selectedItem;
    const timePeriod = tab.querySelector('.time-range-select').pref.value;
    this.installedApps_ = await this.browserProxy_.getInstalledApps(timePeriod);
  },

  /** @private */
  shouldShowInstalledApps_() {
    if (!this.installedAppsFlagEnabled_) {
      return false;
    }
    const haveInstalledApps = this.installedApps_.length > 0;
    chrome.send('metricsHandler:recordBooleanHistogram', [
      'History.ClearBrowsingData.InstalledAppsDialogShown', haveInstalledApps
    ]);
    return haveInstalledApps;
  },

  /**
   * Logs interactions with the installed app dialog to UMA.
   * @private
   */
  recordInstalledAppsInteractions_: function() {
    if (this.installedApps_.length === 0) {
      return;
    }

    const uncheckedAppCount =
        this.installedApps_.filter(app => !app.isChecked).length;
    chrome.metricsPrivate.recordBoolean(
        'History.ClearBrowsingData.InstalledAppExcluded', !!uncheckedAppCount);
    chrome.metricsPrivate.recordCount(
        'History.ClearBrowsingData.InstalledDeselectedNum', uncheckedAppCount);
    chrome.metricsPrivate.recordPercentage(
        'History.ClearBrowsingData.InstalledDeselectedPercent',
        Math.round(100 * uncheckedAppCount / this.installedApps_.length));
  },

  /**
   * Clears browsing data and maybe shows a history notice.
   * @return {!Promise}
   * @private
   */
  clearBrowsingData_: async function() {
    this.clearingInProgress_ = true;
    const tab = this.$.tabs.selectedItem;
    const dataTypes = this.getSelectedDataTypes_(tab);
    const timePeriod = tab.querySelector('.time-range-select').pref.value;

    if (tab.id == 'basic-tab') {
      chrome.metricsPrivate.recordUserAction('ClearBrowsingData_BasicTab');
    } else {
      chrome.metricsPrivate.recordUserAction('ClearBrowsingData_AdvancedTab');
    }

    this.shadowRoot.querySelectorAll('settings-checkbox[no-set-pref]')
        .forEach(checkbox => checkbox.sendPrefChange());

    this.recordInstalledAppsInteractions_();
    const shouldShowNotice = await this.browserProxy_.clearBrowsingData(
        dataTypes, timePeriod, this.installedApps_);
    this.clearingInProgress_ = false;
    this.showHistoryDeletionDialog_ = shouldShowNotice;
    chrome.metricsPrivate.recordMediumTime(
        'History.ClearBrowsingData.TimeSpentInDialog',
        Date.now() - this.dialogOpenedTime_);
    if (!shouldShowNotice) {
      this.closeDialogs_();
    }
  },

  /**
   * Closes clear brtowsing data or installed app dialog if they are open.
   * @private
   */
  closeDialogs_() {
    if (this.$.clearBrowsingDataDialog.open) {
      this.$.clearBrowsingDataDialog.close();
    }
    if (this.$.installedAppsDialog.open) {
      this.$.installedAppsDialog.close();
    }
  },

  /** @private */
  onCancelTap_() {
    this.$.clearBrowsingDataDialog.cancel();
  },

  /**
   * Handles the closing of the notice about other forms of browsing history.
   * @private
   */
  onHistoryDeletionDialogClose_() {
    this.showHistoryDeletionDialog_ = false;
    this.closeDialogs_();
  },

  /**
   * Records an action when the user changes between the basic and advanced tab.
   * @param {!Event} event
   * @private
   */
  recordTabChange_(event) {
    if (event.detail.value == 0) {
      chrome.metricsPrivate.recordUserAction(
          'ClearBrowsingData_SwitchTo_BasicTab');
    } else {
      chrome.metricsPrivate.recordUserAction(
          'ClearBrowsingData_SwitchTo_AdvancedTab');
    }
  },

  /**
   * Called when the user clicks the link in the footer.
   * @param {!Event} e
   * @private
   */
  onSyncDescriptionLinkClicked_(e) {
    if (e.target.tagName === 'A') {
      e.preventDefault();
      if (!this.syncStatus.hasError) {
        chrome.metricsPrivate.recordUserAction('ClearBrowsingData_Sync_Pause');
        this.syncBrowserProxy_.pauseSync();
      } else if (this.isSyncPaused_) {
        chrome.metricsPrivate.recordUserAction('ClearBrowsingData_Sync_SignIn');
        this.syncBrowserProxy_.startSignIn();
      } else {
        if (this.hasPassphraseError_) {
          chrome.metricsPrivate.recordUserAction(
              'ClearBrowsingData_Sync_NavigateToPassphrase');
        } else {
          chrome.metricsPrivate.recordUserAction(
              'ClearBrowsingData_Sync_NavigateToError');
        }
        // In any other error case, navigate to the sync page.
        Router.getInstance().navigateTo(routes.SYNC);
      }
    }
  },

  /**
   * @return {boolean}
   * @private
   */
  computeIsSyncPaused_() {
    return !!this.syncStatus.hasError &&
        this.syncStatus.statusAction === StatusAction.REAUTHENTICATE;
  },

  /**
   * @return {boolean}
   * @private
   */
  computeHasPassphraseError_() {
    return !!this.syncStatus.hasError &&
        this.syncStatus.statusAction === StatusAction.ENTER_PASSPHRASE;
  },

  /**
   * @return {boolean}
   * @private
   */
  computeHasOtherError_() {
    return this.syncStatus !== undefined && !!this.syncStatus.hasError &&
        !this.isSyncPaused_ && !this.hasPassphraseError_;
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowFooter_() {
    let showFooter = false;
    // <if expr="not chromeos">
    showFooter = !!this.syncStatus && !!this.syncStatus.signedIn;
    // </if>
    return showFooter;
  },

  /**
   * @return {!Promise}
   * @private
   */
  onClearBrowsingDataClick_: async function() {
    await this.getInstalledApps_();
    if (this.shouldShowInstalledApps_()) {
      replaceDialog(this.$.clearBrowsingDataDialog, this.$.installedAppsDialog);
    } else {
      await this.clearBrowsingData_();
    }
  },

  /** @private */
  hideInstalledApps_() {
    replaceDialog(this.$.installedAppsDialog, this.$.clearBrowsingDataDialog);
  },

  /**
   * Handles the tap confirm button in installed apps.
   * @private
   */
  onInstalledAppsConfirmClick_: async function() {
    await this.clearBrowsingData_();
  }
});
