// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-clear-browsing-data-dialog' allows the user to
 * delete browsing data that has been cached by Chromium.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/cr_tabs/cr_tabs.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-pages/iron-pages.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import './history_deletion_dialog.js';
import './passwords_deletion_dialog.js';
import './installed_app_checkbox.js';
import '../controls/settings_checkbox.js';
import '../icons.js';
import '../settings_shared_css.js';

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {IronA11yAnnouncer} from 'chrome://resources/polymer/v3_0/iron-a11y-announcer/iron-a11y-announcer.js';
import {IronPagesElement} from 'chrome://resources/polymer/v3_0/iron-pages/iron-pages.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PrefControlBehaviorInterface} from '../controls/pref_control_behavior.js';
import {DropdownMenuOptionList} from '../controls/settings_dropdown_menu.js';
import {loadTimeData} from '../i18n_setup.js';
import {StatusAction, SyncBrowserProxy, SyncBrowserProxyImpl, SyncStatus} from '../people_page/sync_browser_proxy.js';
import {routes} from '../route.js';
import {Route, RouteObserverMixin, RouteObserverMixinInterface, Router} from '../router.js';

import {ClearBrowsingDataBrowserProxy, ClearBrowsingDataBrowserProxyImpl, InstalledApp} from './clear_browsing_data_browser_proxy.js';

/**
 * InstalledAppsDialogActions enum.
 * These values are persisted to logs and should not be renumbered or
 * re-used.
 * See tools/metrics/histograms/enums.xml.
 */
enum InstalledAppsDialogActions {
  CLOSE = 0,
  CANCEL_BUTTON = 1,
  CLEAR_BUTTON = 2,
}

/**
 * @param dialog the dialog to close
 * @param isLast whether this is the last CBD-related dialog
 */
function closeDialog(dialog: CrDialogElement, isLast: boolean) {
  // If this is not the last dialog, then stop the 'close' event from
  // propagating so that other (following) dialogs don't get closed as well.
  if (!isLast) {
    dialog.addEventListener('close', e => {
      e.stopPropagation();
    }, {once: true});
  }
  dialog.close();
}

/**
 * @param oldDialog the dialog to close
 * @param newDialog the dialog to open
 */
function replaceDialog(oldDialog: CrDialogElement, newDialog: CrDialogElement) {
  closeDialog(oldDialog, false);
  if (!newDialog.open) {
    newDialog.showModal();
  }
}

type UpdateSyncStateEvent = {
  signedIn: boolean,
  syncConsented: boolean,
  syncingHistory: boolean,
  shouldShowCookieException: boolean,
  isNonGoogleDse: boolean,
  nonGoogleSearchHistoryString: string,
};

// TODO(crbug.com/1234307): Remove when settings_checkbox.js is migrated to
// TypeScript.
interface SettingsCheckboxElement extends HTMLElement {
  checked: boolean;
  pref: chrome.settingsPrivate.PrefObject;
  sendPrefChange(): void;
}

interface SettingsClearBrowsingDataDialogElement {
  $: {
    clearBrowsingDataDialog: CrDialogElement,
    installedAppsDialog: CrDialogElement,
    tabs: IronPagesElement,
  };
}

const SettingsClearBrowsingDataDialogElementBase =
    mixinBehaviors(
        [I18nBehavior, WebUIListenerBehavior],
        RouteObserverMixin(PolymerElement)) as {
      new (): PolymerElement & WebUIListenerBehavior & I18nBehavior &
      RouteObserverMixinInterface
    };

class SettingsClearBrowsingDataDialogElement extends
    SettingsClearBrowsingDataDialogElementBase {
  static get is() {
    return 'settings-clear-browsing-data-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Preferences state.
       */
      prefs: {
        type: Object,
        notify: true,
      },

      /**
       * The current sync status, supplied by SyncBrowserProxy.
       */
      syncStatus: Object,

      /**
       * Results of browsing data counters, keyed by the suffix of
       * the corresponding data type deletion preference, as reported
       * by the C++ side.
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

      clearingInProgress_: {
        type: Boolean,
        value: false,
      },

      clearingDataAlertString_: {
        type: String,
        value: '',
      },

      clearButtonDisabled_: {
        type: Boolean,
        value: false,
      },

      isSupervised_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isSupervised');
        },
      },

      showHistoryDeletionDialog_: {
        type: Boolean,
        value: false,
      },

      showPasswordsDeletionDialogLater_: {
        type: Boolean,
        value: false,
      },

      showPasswordsDeletionDialog_: {
        type: Boolean,
        value: false,
      },

      isSignedIn_: {
        type: Boolean,
        value: false,
      },

      isSyncConsented_: {
        type: Boolean,
        value: false,
      },

      isSyncingHistory_: {
        type: Boolean,
        value: false,
      },

      shouldShowCookieException_: {
        type: Boolean,
        value: false,
      },

      isSyncPaused_: {
        type: Boolean,
        value: false,
        computed: 'computeIsSyncPaused_(syncStatus)',
      },

      hasPassphraseError_: {
        type: Boolean,
        value: false,
        computed: 'computeHasPassphraseError_(syncStatus)',
      },

      hasOtherSyncError_: {
        type: Boolean,
        value: false,
        computed:
            'computeHasOtherError_(syncStatus, isSyncPaused_, hasPassphraseError_)',
      },

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
       */
      installedApps_: {
        type: Array,
        value: () => [],
      },

      installedAppsFlagEnabled_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('installedAppsInCbd'),
      },

      searchHistoryLinkFlagEnabled_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('searchHistoryLink'),
      },

      googleSearchHistoryString_: {
        type: String,
        computed: 'computeGoogleSearchHistoryString_(isNonGoogleDse_)',
      },

      isNonGoogleDse_: {
        type: Boolean,
        value: false,
      },

      nonGoogleSearchHistoryString_: String,
    };
  }

  // TODO(dpapad): make |syncStatus| private.
  syncStatus: SyncStatus|undefined;
  private counters_: {[k: string]: string};
  private clearFromOptions_: DropdownMenuOptionList;
  private clearingInProgress_: boolean;
  private clearingDataAlertString_: string;
  private clearButtonDisabled_: boolean;
  private isSupervised_: boolean;
  private showHistoryDeletionDialog_: boolean;
  private showPasswordsDeletionDialogLater_: boolean;
  private showPasswordsDeletionDialog_: boolean;
  private isSignedIn_: boolean;
  private isSyncConsented_: boolean;
  private isSyncingHistory_: boolean;
  private shouldShowCookieException_: boolean;
  private isSyncPaused_: boolean;
  private hasPassphraseError_: boolean;
  private hasOtherSyncError_: boolean;
  private tabsNames_: Array<string>;
  private installedApps_: Array<InstalledApp>;
  private installedAppsFlagEnabled_: boolean;
  private searchHistoryLinkFlagEnabled_: boolean;
  private googleSearchHistoryString_: string;
  private isNonGoogleDse_: boolean;
  private nonGoogleSearchHistoryString_: string;

  private browserProxy_: ClearBrowsingDataBrowserProxy =
      ClearBrowsingDataBrowserProxyImpl.getInstance();
  private syncBrowserProxy_: SyncBrowserProxy =
      SyncBrowserProxyImpl.getInstance();

  ready() {
    super.ready();

    this.syncBrowserProxy_.getSyncStatus().then(
        this.handleSyncStatus_.bind(this));
    this.addWebUIListener(
        'sync-status-changed', this.handleSyncStatus_.bind(this));

    this.addWebUIListener(
        'update-sync-state', this.updateSyncState_.bind(this));
    this.addWebUIListener(
        'update-counter-text', this.updateCounterText_.bind(this));

    this.addEventListener(
        'settings-boolean-control-change', this.updateClearButtonState_);
  }

  connectedCallback() {
    super.connectedCallback();

    this.browserProxy_.initialize().then(() => {
      this.$.clearBrowsingDataDialog.showModal();
    });
  }

  /**
   * Handler for when the sync state is pushed from the browser.
   */
  private handleSyncStatus_(syncStatus: SyncStatus) {
    this.syncStatus = syncStatus;
  }

  /**
   * @return Whether either clearing is in progress or no data type is selected.
   */
  private isClearButtonDisabled_(
      clearingInProgress: boolean, clearButtonDisabled: boolean): boolean {
    return clearingInProgress || clearButtonDisabled;
  }

  /**
   * Disables the Clear Data button if no data type is selected.
   */
  private updateClearButtonState_() {
    // on-select-item-changed gets called with undefined during a tab change.
    // https://github.com/PolymerElements/iron-selector/issues/95
    const tab = this.$.tabs.selectedItem;
    if (!tab) {
      return;
    }
    this.clearButtonDisabled_ =
        this.getSelectedDataTypes_(tab as HTMLElement).length === 0;
  }

  /**
   * Record visits to the CBD dialog.
   *
   * RouteObserverMixin
   */
  currentRouteChanged(currentRoute: Route) {
    if (currentRoute === routes.CLEAR_BROWSER_DATA) {
      chrome.metricsPrivate.recordUserAction('ClearBrowsingData_DialogCreated');
    }
  }

  /**
   * Updates the history description to show the relevant information
   * depending on sync and signin state.
   */
  private updateSyncState_(event: UpdateSyncStateEvent) {
    this.isSignedIn_ = event.signedIn;
    this.isSyncConsented_ = event.syncConsented;
    this.isSyncingHistory_ = event.syncingHistory;
    this.shouldShowCookieException_ = event.shouldShowCookieException;
    this.$.clearBrowsingDataDialog.classList.add('fully-rendered');
    this.isNonGoogleDse_ = event.isNonGoogleDse;
    this.nonGoogleSearchHistoryString_ = event.nonGoogleSearchHistoryString;
  }

  /** Choose a label for the history checkbox. */
  private browsingCheckboxLabel_(
      isSyncConsented: boolean, isSyncingHistory: boolean,
      hasSyncError: boolean, historySummary: string,
      historySummarySignedIn: string, historySummarySignedInNoLink: string,
      historySummarySynced: string): string {
    if (this.searchHistoryLinkFlagEnabled_) {
      return isSyncingHistory ? historySummarySignedInNoLink : historySummary;
    } else if (isSyncingHistory && !hasSyncError) {
      return historySummarySynced;
    } else if (isSyncConsented && !this.isSyncPaused_) {
      return historySummarySignedIn;
    }
    return historySummary;
  }

  /** Choose a label for the cookie checkbox. */
  private cookiesCheckboxLabel_(
      shouldShowCookieException: boolean, cookiesSummary: string,
      cookiesSummarySignedIn: string): string {
    if (shouldShowCookieException) {
      return cookiesSummarySignedIn;
    }
    return cookiesSummary;
  }

  /**
   * Updates the text of a browsing data counter corresponding to the given
   * preference.
   * @param prefName Browsing data type deletion preference.
   * @param text The text with which to update the counter
   */
  private updateCounterText_(prefName: string, text: string) {
    // Data type deletion preferences are named "browser.clear_data.<datatype>".
    // Strip the common prefix, i.e. use only "<datatype>".
    const matches = prefName.match(/^browser\.clear_data\.(\w+)$/)!;
    this.set('counters_.' + assert(matches[1]), text);
  }

  /**
   * @return A list of selected data types.
   */
  private getSelectedDataTypes_(tab: HTMLElement): Array<string> {
    const checkboxes = tab.querySelectorAll('settings-checkbox') as
        NodeListOf<SettingsCheckboxElement>;
    const dataTypes: Array<string> = [];
    checkboxes.forEach((checkbox) => {
      if (checkbox.checked && !checkbox.hidden) {
        dataTypes.push(checkbox.pref.key);
      }
    });
    return dataTypes;
  }

  /**
   * Gets a list of top 5 installed apps that have been launched
   * within the time period selected. This is used to warn the user
   * that data for these apps will be cleared as well, and offers
   * them the option to exclude deletion of this data.
   */
  private async getInstalledApps_() {
    const tab = this.$.tabs.selectedItem as HTMLElement;
    // TODO(crbug.com/1234307): Cast to SettingsDropdownMenuElement when
    // settings_dropdown_menu.js is migrated to TypeScript.
    const timePeriod = (tab.querySelector('.time-range-select') as unknown as
                        PrefControlBehaviorInterface)
                           .pref!.value;
    this.installedApps_ = await this.browserProxy_.getInstalledApps(timePeriod);
  }

  private shouldShowInstalledApps_(): boolean {
    if (!this.installedAppsFlagEnabled_) {
      return false;
    }
    const haveInstalledApps = this.installedApps_.length > 0;
    chrome.send('metricsHandler:recordBooleanHistogram', [
      'History.ClearBrowsingData.InstalledAppsDialogShown', haveInstalledApps
    ]);
    return haveInstalledApps;
  }

  /** Logs interactions with the installed app dialog to UMA. */
  private recordInstalledAppsInteractions_() {
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
  }

  /** Clears browsing data and maybe shows a history notice. */
  private async clearBrowsingData_() {
    this.clearingInProgress_ = true;
    this.clearingDataAlertString_ = loadTimeData.getString('clearingData');
    const tab = this.$.tabs.selectedItem as HTMLElement;
    const dataTypes = this.getSelectedDataTypes_(tab);
    const timePeriod = (tab.querySelector('.time-range-select') as unknown as
                        PrefControlBehaviorInterface)
                           .pref!.value;

    if (tab.id === 'basic-tab') {
      chrome.metricsPrivate.recordUserAction('ClearBrowsingData_BasicTab');
    } else {
      chrome.metricsPrivate.recordUserAction('ClearBrowsingData_AdvancedTab');
    }

    (this.shadowRoot!.querySelectorAll('settings-checkbox[no-set-pref]') as
     NodeListOf<SettingsCheckboxElement>)
        .forEach(checkbox => checkbox.sendPrefChange());

    const {showHistoryNotice, showPasswordsNotice} =
        await this.browserProxy_.clearBrowsingData(
            dataTypes, timePeriod, this.installedApps_);
    this.clearingInProgress_ = false;
    IronA11yAnnouncer.requestAvailability();
    this.dispatchEvent(new CustomEvent('iron-announce', {
      bubbles: true,
      composed: true,
      detail: {text: loadTimeData.getString('clearedData')}
    }));
    this.showHistoryDeletionDialog_ = showHistoryNotice;
    // If both the history notice and the passwords notice should be shown, show
    // the history notice first, and then show the passwords notice once the
    // history notice gets closed.
    this.showPasswordsDeletionDialog_ =
        showPasswordsNotice && !showHistoryNotice;
    this.showPasswordsDeletionDialogLater_ =
        showPasswordsNotice && showHistoryNotice;

    // Close the clear browsing data or installed apps dialog if they are open.
    const isLastDialog = !showHistoryNotice && !showPasswordsNotice;
    if (this.$.clearBrowsingDataDialog.open) {
      closeDialog(this.$.clearBrowsingDataDialog, isLastDialog);
    }
    if (this.$.installedAppsDialog.open) {
      closeDialog(this.$.installedAppsDialog, isLastDialog);
    }
  }

  private onCancelTap_() {
    this.$.clearBrowsingDataDialog.cancel();
  }

  /**
   * Handles the closing of the notice about other forms of browsing history.
   */
  private onHistoryDeletionDialogClose_(e: Event) {
    this.showHistoryDeletionDialog_ = false;
    if (this.showPasswordsDeletionDialogLater_) {
      // Stop the close event from propagating further and also automatically
      // closing other dialogs.
      e.stopPropagation();
      this.showPasswordsDeletionDialogLater_ = false;
      this.showPasswordsDeletionDialog_ = true;
    }
  }

  /**
   * Handles the closing of the notice about incomplete passwords deletion.
   */
  private onPasswordsDeletionDialogClose_() {
    this.showPasswordsDeletionDialog_ = false;
  }

  /**
   * Records an action when the user changes between the basic and advanced tab.
   */
  private recordTabChange_(event: CustomEvent<{value: number}>) {
    if (event.detail.value === 0) {
      chrome.metricsPrivate.recordUserAction(
          'ClearBrowsingData_SwitchTo_BasicTab');
    } else {
      chrome.metricsPrivate.recordUserAction(
          'ClearBrowsingData_SwitchTo_AdvancedTab');
    }
  }

  // <if expr="not chromeos">
  /** Called when the user clicks the link in the footer. */
  private onSyncDescriptionLinkClicked_(e: Event) {
    if ((e.target as HTMLElement).tagName === 'A') {
      e.preventDefault();
      if (!this.syncStatus!.hasError) {
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
  }
  // </if>

  private computeIsSyncPaused_(): boolean {
    return !!this.syncStatus!.hasError &&
        !this.syncStatus!.hasUnrecoverableError &&
        this.syncStatus!.statusAction === StatusAction.REAUTHENTICATE;
  }

  private computeHasPassphraseError_(): boolean {
    return !!this.syncStatus!.hasError &&
        this.syncStatus!.statusAction === StatusAction.ENTER_PASSPHRASE;
  }

  private computeHasOtherError_(): boolean {
    return this.syncStatus !== undefined && !!this.syncStatus!.hasError &&
        !this.isSyncPaused_ && !this.hasPassphraseError_;
  }

  private computeGoogleSearchHistoryString_(isNonGoogleDse: boolean): string {
    return isNonGoogleDse ?
        this.i18nAdvanced('clearGoogleSearchHistoryNonGoogleDse') :
        this.i18nAdvanced('clearGoogleSearchHistoryGoogleDse');
  }

  private shouldShowGoogleSearchHistoryLabel_(isSignedIn: boolean): boolean {
    return this.searchHistoryLinkFlagEnabled_ && isSignedIn;
  }

  private shouldShowNonGoogleSearchHistoryLabel_(isNonGoogleDse: boolean):
      boolean {
    return this.searchHistoryLinkFlagEnabled_ && isNonGoogleDse;
  }

  private shouldShowFooter_(): boolean {
    let showFooter = false;
    // <if expr="not chromeos">
    showFooter = !!this.syncStatus && !!this.syncStatus!.signedIn;
    // </if>
    return showFooter;
  }

  private async onClearBrowsingDataClick_() {
    await this.getInstalledApps_();
    if (this.shouldShowInstalledApps_()) {
      replaceDialog(this.$.clearBrowsingDataDialog, this.$.installedAppsDialog);
    } else {
      await this.clearBrowsingData_();
    }
  }

  private hideInstalledApps_() {
    chrome.metricsPrivate.recordEnumerationValue(
        'History.ClearBrowsingData.InstalledAppsDialogAction',
        InstalledAppsDialogActions.CLOSE,
        Object.keys(InstalledAppsDialogActions).length);
    replaceDialog(this.$.installedAppsDialog, this.$.clearBrowsingDataDialog);
  }

  private onCancelInstalledApps_() {
    chrome.metricsPrivate.recordEnumerationValue(
        'History.ClearBrowsingData.InstalledAppsDialogAction',
        InstalledAppsDialogActions.CANCEL_BUTTON,
        Object.keys(InstalledAppsDialogActions).length);
    replaceDialog(this.$.installedAppsDialog, this.$.clearBrowsingDataDialog);
  }

  /** Handles the tap confirm button in installed apps. */
  private async onInstalledAppsConfirmClick_() {
    chrome.metricsPrivate.recordEnumerationValue(
        'History.ClearBrowsingData.InstalledAppsDialogAction',
        InstalledAppsDialogActions.CLEAR_BUTTON,
        Object.keys(InstalledAppsDialogActions).length);
    this.recordInstalledAppsInteractions_();
    await this.clearBrowsingData_();
  }
}

customElements.define(
    SettingsClearBrowsingDataDialogElement.is,
    SettingsClearBrowsingDataDialogElement);
