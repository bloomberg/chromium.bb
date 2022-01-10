// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/js/util.m.js';
import '//resources/cr_elements/cr_button/cr_button.m.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.m.js';
import '//resources/cr_elements/cr_input/cr_input.m.js';
import '//resources/cr_elements/cr_link_row/cr_link_row.js';
import '//resources/cr_elements/icons.m.js';
import '//resources/cr_elements/shared_style_css.m.js';
import '//resources/cr_elements/shared_vars_css.m.js';
import '//resources/cr_elements/cr_expand_button/cr_expand_button.m.js';
import '//resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import './sync_account_control.js';
import './sync_encryption_options.js';
import '../privacy_page/personalization_options.js';
import '../settings_shared_css.js';
import '../settings_vars_css.js';
// <if expr="not chromeos">
import '//resources/cr_elements/cr_toast/cr_toast.js';
// </if>

import {CrDialogElement} from '//resources/cr_elements/cr_dialog/cr_dialog.m.js';
import {CrInputElement} from '//resources/cr_elements/cr_input/cr_input.m.js';
import {assert, assertNotReached} from '//resources/js/assert.m.js';
import {focusWithoutInk} from '//resources/js/cr/ui/focus_without_ink.m.js';
import {WebUIListenerMixin, WebUIListenerMixinInterface} from '//resources/js/web_ui_listener_mixin.js';
import {flush, html, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {I18nMixin, I18nMixinInterface} from 'chrome://resources/js/i18n_mixin.js';

import {loadTimeData} from '../i18n_setup.js';

// <if expr="chromeos">
import {SettingsPersonalizationOptionsElement} from '../privacy_page/personalization_options.js';
// </if>

import {Route, RouteObserverMixin, RouteObserverMixinInterface, Router} from '../router.js';

import {PageStatus, StatusAction, SyncBrowserProxy, SyncBrowserProxyImpl, SyncPrefs, SyncStatus} from './sync_browser_proxy.js';

// <if expr="chromeos">
import {SettingsSyncEncryptionOptionsElement} from './sync_encryption_options.js';
// </if>

// TODO(rbpotter): Remove this typedef when this file is no longer needed by OS
// Settings.
type SyncRoutes = {
  BASIC: Route,
  PEOPLE: Route,
  SYNC: Route,
  SYNC_ADVANCED: Route,
  OS_SYNC: Route,
};

function getSyncRoutes(): SyncRoutes {
  const router = Router.getInstance();
  return router.getRoutes() as SyncRoutes;
}

type FocusConfig = Map<string, (string|(() => void))>;

/**
 * @fileoverview
 * 'settings-sync-page' is the settings page containing sync settings.
 */

const SettingsSyncPageElementBase =
    RouteObserverMixin(WebUIListenerMixin(I18nMixin(PolymerElement))) as {
      new (): PolymerElement & WebUIListenerMixinInterface &
      I18nMixinInterface & RouteObserverMixinInterface
    };

export class SettingsSyncPageElement extends SettingsSyncPageElementBase {
  static get is() {
    return 'settings-sync-page';
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

      focusConfig: {
        type: Object,
        observer: 'onFocusConfigChange_',
      },

      pageStatusEnum_: {
        type: Object,
        value: PageStatus,
        readOnly: true,
      },

      /**
       * The current page status. Defaults to |CONFIGURE| such that the
       * searching algorithm can search useful content when the page is not
       * visible to the user.
       */
      pageStatus_: {
        type: String,
        value: PageStatus.CONFIGURE,
      },

      /**
       * Dictionary defining page visibility.
       * TODO(dpapad): Restore the type information here
       * (PrivacyPageVisibility), when this file is no longer shared with
       * chrome://os-settings.
       */
      pageVisibility: Object,

      /**
       * The current sync preferences, supplied by SyncBrowserProxy.
       */
      syncPrefs: Object,

      syncStatus: Object,

      dataEncrypted_: {
        type: Boolean,
        computed: 'computeDataEncrypted_(syncPrefs.encryptAllData)'
      },

      encryptionExpanded_: {
        type: Boolean,
        value: false,
      },

      /** If true, override |encryptionExpanded_| to be true. */
      forceEncryptionExpanded: {
        type: Boolean,
        value: false,
      },

      /**
       * The existing passphrase input field value.
       */
      existingPassphrase_: {
        type: String,
        value: '',
      },

      signedIn_: {
        type: Boolean,
        value: true,
        computed: 'computeSignedIn_(syncStatus.signedIn)',
      },

      syncDisabledByAdmin_: {
        type: Boolean,
        value: false,
        computed: 'computeSyncDisabledByAdmin_(syncStatus.managed)',
      },

      syncSectionDisabled_: {
        type: Boolean,
        value: false,
        computed: 'computeSyncSectionDisabled_(' +
            'syncStatus.signedIn, syncStatus.disabled, ' +
            'syncStatus.hasError, syncStatus.statusAction, ' +
            'syncPrefs.trustedVaultKeysRequired)',
      },

      // <if expr="not lacros">
      showSetupCancelDialog_: {
        type: Boolean,
        value: false,
      },
      // </if>

      enterPassphraseLabel_: {
        type: String,
        computed: 'computeEnterPassphraseLabel_(syncPrefs.encryptAllData,' +
            'syncPrefs.explicitPassphraseTime)',
      },

      existingPassphraseLabel_: {
        type: String,
        computed: 'computeExistingPassphraseLabel_(syncPrefs.encryptAllData,' +
            'syncPrefs.explicitPassphraseTime)',
      },
    };
  }

  static get observers() {
    return [
      'expandEncryptionIfNeeded_(dataEncrypted_, forceEncryptionExpanded)',
    ];
  }

  focusConfig: FocusConfig;
  private pageStatus_: PageStatus
  syncPrefs?: SyncPrefs;
  syncStatus: SyncStatus;
  private dataEncrypted_: boolean;
  private encryptionExpanded_: boolean;
  forceEncryptionExpanded: boolean;
  private existingPassphrase_: string;
  private signedIn_: boolean;
  private syncDisabledByAdmin_: boolean;
  private syncSectionDisabled_: boolean;
  // <if expr="not lacros">
  private showSetupCancelDialog_: boolean;
  // </if>
  private enterPassphraseLabel_: string;
  private existingPassphraseLabel_: string;

  private browserProxy_: SyncBrowserProxy = SyncBrowserProxyImpl.getInstance();
  private collapsibleSectionsInitialized_: boolean;
  private didAbort_: boolean;
  private setupCancelConfirmed_: boolean;
  private beforeunloadCallback_: ((e: Event) => void)|null;
  private unloadCallback_: (() => void)|null;

  constructor() {
    super();

    /**
     * The beforeunload callback is used to show the 'Leave site' dialog. This
     * makes sure that the user has the chance to go back and confirm the sync
     * opt-in before leaving.
     *
     * This property is non-null if the user is currently navigated on the sync
     * settings route.
     */
    this.beforeunloadCallback_ = null;

    /**
     * The unload callback is used to cancel the sync setup when the user hits
     * the browser back button after arriving on the page.
     * Note = Cases like closing the tab or reloading don't need to be handled;
     * because they are already caught in |PeopleHandler::~PeopleHandler|
     * from the C++ code.
     */
    this.unloadCallback_ = null;

    /**
     * Whether the initial layout for collapsible sections has been computed. It
     * is computed only once; the first time the sync status is updated.
     */
    this.collapsibleSectionsInitialized_ = false;

    /**
     * Whether the user decided to abort sync.
     */
    this.didAbort_ = true;

    /**
     * Whether the user confirmed the cancellation of sync.
     */
    this.setupCancelConfirmed_ = false;
  }

  connectedCallback() {
    super.connectedCallback();

    this.addWebUIListener(
        'page-status-changed', this.handlePageStatusChanged_.bind(this));
    this.addWebUIListener(
        'sync-prefs-changed', this.handleSyncPrefsChanged_.bind(this));

    const router = Router.getInstance();
    if (router.getCurrentRoute() === getSyncRoutes().SYNC) {
      this.onNavigateToPage_();
    }
  }

  disconnectedCallback() {
    super.disconnectedCallback();

    const router = Router.getInstance();
    if (getSyncRoutes().SYNC.contains(router.getCurrentRoute())) {
      this.onNavigateAwayFromPage_();
    }

    if (this.beforeunloadCallback_) {
      window.removeEventListener('beforeunload', this.beforeunloadCallback_);
      this.beforeunloadCallback_ = null;
    }
    if (this.unloadCallback_) {
      window.removeEventListener('unload', this.unloadCallback_);
      this.unloadCallback_ = null;
    }
  }

  // <if expr="chromeos">
  /**
   * @return The encryption options SettingsSyncEncryptionOptionsElement.
   */
  getEncryptionOptions(): SettingsSyncEncryptionOptionsElement|null {
    return this.shadowRoot!.querySelector('settings-sync-encryption-options');
  }

  /**
   * @return The personalization options SettingsPersonalizationOptionsElement.
   */
  getPersonalizationOptions(): SettingsPersonalizationOptionsElement|null {
    return this.shadowRoot!.querySelector('settings-personalization-options');
  }
  // </if>

  // <if expr="chromeos or lacros">
  private shouldShowLacrosSideBySideWarning_(): boolean {
    return loadTimeData.getBoolean('shouldShowLacrosSideBySideWarning');
  }
  // </if>

  private showActivityControls_(): boolean {
    // <if expr="chromeos">
    if (loadTimeData.getBoolean('syncSettingsCategorizationEnabled')) {
      // Should be hidden in OS settings.
      return !loadTimeData.getBoolean('isOSSettings');
    }
    // </if>
    return true;
  }

  private computeSignedIn_(): boolean {
    return !!this.syncStatus.signedIn;
  }

  private computeSyncSectionDisabled_(): boolean {
    return this.syncStatus !== undefined &&
        (!this.syncStatus.signedIn || !!this.syncStatus.disabled ||
         (!!this.syncStatus.hasError &&
          this.syncStatus.statusAction !== StatusAction.ENTER_PASSPHRASE &&
          this.syncStatus.statusAction !==
              StatusAction.RETRIEVE_TRUSTED_VAULT_KEYS));
  }

  private computeSyncDisabledByAdmin_(): boolean {
    return this.syncStatus !== undefined && !!this.syncStatus.managed;
  }

  private getSyncAdvancedPageRoute_(): Route {
    // <if expr="chromeos">
    if (loadTimeData.getBoolean('syncSettingsCategorizationEnabled') &&
        loadTimeData.getBoolean('isOSSettings')) {
      // In OS settings on ChromeOS a different page is used to show the list of
      // sync data types.
      return getSyncRoutes().OS_SYNC;
    }
    // </if>

    return getSyncRoutes().SYNC_ADVANCED;
  }

  private onFocusConfigChange_() {
    this.focusConfig.set(this.getSyncAdvancedPageRoute_().path, () => {
      focusWithoutInk(
          assert(this.shadowRoot!.querySelector('#sync-advanced-row')!));
    });
  }

  // <if expr="not lacros">
  private onSetupCancelDialogBack_() {
    this.shadowRoot!.querySelector<CrDialogElement>(
                        '#setupCancelDialog')!.cancel();
    chrome.metricsPrivate.recordUserAction(
        'Signin_Signin_CancelCancelAdvancedSyncSettings');
  }

  private onSetupCancelDialogConfirm_() {
    this.setupCancelConfirmed_ = true;
    this.shadowRoot!.querySelector<CrDialogElement>(
                        '#setupCancelDialog')!.close();
    const router = Router.getInstance();
    router.navigateTo(getSyncRoutes().BASIC);
    chrome.metricsPrivate.recordUserAction(
        'Signin_Signin_ConfirmCancelAdvancedSyncSettings');
  }

  private onSetupCancelDialogClose_() {
    this.showSetupCancelDialog_ = false;
  }
  // </if>

  currentRouteChanged() {
    const router = Router.getInstance();
    if (router.getCurrentRoute() === getSyncRoutes().SYNC) {
      this.onNavigateToPage_();
      return;
    }

    if (getSyncRoutes().SYNC.contains(router.getCurrentRoute())) {
      return;
    }

    const searchParams =
        Router.getInstance().getQueryParameters().get('search');
    if (searchParams) {
      // User navigated away via searching. Cancel sync without showing
      // confirmation dialog.
      this.onNavigateAwayFromPage_();
      return;
    }

    // On Lacros, turning off sync is not supported yet.
    // TODO(https://crbug.com/1217645): Enable the cancel dialog.
    // <if expr="not lacros">
    const userActionCancelsSetup = this.syncStatus &&
        this.syncStatus.firstSetupInProgress && this.didAbort_;
    if (userActionCancelsSetup && !this.setupCancelConfirmed_) {
      chrome.metricsPrivate.recordUserAction(
          'Signin_Signin_BackOnAdvancedSyncSettings');
      // Show the 'Cancel sync?' dialog.
      // Yield so that other |currentRouteChanged| observers are called,
      // before triggering another navigation (and another round of observers
      // firing). Triggering navigation from within an observer leads to some
      // undefined behavior and runtime errors.
      requestAnimationFrame(() => {
        router.navigateTo(getSyncRoutes().SYNC);
        this.showSetupCancelDialog_ = true;
        // Flush to make sure that the setup cancel dialog is attached.
        flush();
        this.shadowRoot!.querySelector<CrDialogElement>(
                            '#setupCancelDialog')!.showModal();
      });
      return;
    }
    // </if>

    // Reset variable.
    this.setupCancelConfirmed_ = false;

    this.onNavigateAwayFromPage_();
  }

  private isStatus_(expectedPageStatus: PageStatus): boolean {
    return expectedPageStatus === this.pageStatus_;
  }

  private onNavigateToPage_() {
    const router = Router.getInstance();
    assert(router.getCurrentRoute() === getSyncRoutes().SYNC);
    if (this.beforeunloadCallback_) {
      return;
    }

    this.collapsibleSectionsInitialized_ = false;

    // Display loading page until the settings have been retrieved.
    this.pageStatus_ = PageStatus.SPINNER;

    this.browserProxy_.didNavigateToSyncPage();

    this.beforeunloadCallback_ = event => {
      // When the user tries to leave the sync setup, show the 'Leave site'
      // dialog.
      if (this.syncStatus && this.syncStatus.firstSetupInProgress) {
        event.preventDefault();

        chrome.metricsPrivate.recordUserAction(
            'Signin_Signin_AbortAdvancedSyncSettings');
      }
    };
    window.addEventListener('beforeunload', this.beforeunloadCallback_);

    this.unloadCallback_ = this.onNavigateAwayFromPage_.bind(this);
    window.addEventListener('unload', this.unloadCallback_);
  }

  private onNavigateAwayFromPage_() {
    if (!this.beforeunloadCallback_) {
      return;
    }

    // Reset the status to CONFIGURE such that the searching algorithm can
    // search useful content when the page is not visible to the user.
    this.pageStatus_ = PageStatus.CONFIGURE;

    this.browserProxy_.didNavigateAwayFromSyncPage(this.didAbort_);

    window.removeEventListener('beforeunload', this.beforeunloadCallback_);
    this.beforeunloadCallback_ = null;

    if (this.unloadCallback_) {
      window.removeEventListener('unload', this.unloadCallback_);
      this.unloadCallback_ = null;
    }
  }

  /**
   * Handler for when the sync preferences are updated.
   */
  private handleSyncPrefsChanged_(syncPrefs: SyncPrefs) {
    this.syncPrefs = syncPrefs;
    this.pageStatus_ = PageStatus.CONFIGURE;
  }

  private onActivityControlsClick_() {
    chrome.metricsPrivate.recordUserAction('Sync_OpenActivityControlsPage');
    this.browserProxy_.openActivityControlsUrl();
    window.open(loadTimeData.getString('activityControlsUrl'));
  }

  private onSyncDashboardLinkClick_() {
    window.open(loadTimeData.getString('syncDashboardUrl'));
  }

  private computeDataEncrypted_(): boolean {
    return !!this.syncPrefs && this.syncPrefs.encryptAllData;
  }

  private computeEnterPassphraseLabel_(): string {
    if (!this.syncPrefs || !this.syncPrefs.encryptAllData) {
      return '';
    }

    if (!this.syncPrefs.explicitPassphraseTime) {
      // TODO(crbug.com/1207432): There's no reason why this dateless label
      // shouldn't link to 'syncErrorsHelpUrl' like the other one.
      return this.i18n('enterPassphraseLabel');
    }

    return this.i18nAdvanced('enterPassphraseLabelWithDate', {
      tags: ['a'],
      substitutions: [
        loadTimeData.getString('syncErrorsHelpUrl'),
        this.syncPrefs.explicitPassphraseTime
      ]
    });
  }

  private computeExistingPassphraseLabel_(): string {
    if (!this.syncPrefs || !this.syncPrefs.encryptAllData) {
      return '';
    }

    if (!this.syncPrefs.explicitPassphraseTime) {
      return this.i18n('existingPassphraseLabel');
    }

    return this.i18n(
        'existingPassphraseLabelWithDate',
        this.syncPrefs.explicitPassphraseTime);
  }

  /**
   * Whether the encryption dropdown should be expanded by default.
   */
  private expandEncryptionIfNeeded_() {
    // Force the dropdown to expand.
    if (this.forceEncryptionExpanded) {
      this.forceEncryptionExpanded = false;
      this.encryptionExpanded_ = true;
      return;
    }

    this.encryptionExpanded_ = this.dataEncrypted_;
  }

  private onResetSyncClick_(event: Event) {
    if ((event.target as HTMLElement).tagName === 'A') {
      // Stop the propagation of events as the |cr-expand-button|
      // prevents the default which will prevent the navigation to the link.
      event.stopPropagation();
    }
  }

  /**
   * Sends the user-entered existing password to re-enable sync.
   */
  private onSubmitExistingPassphraseTap_(e: KeyboardEvent) {
    if (e.type === 'keypress' && e.key !== 'Enter') {
      return;
    }

    this.browserProxy_.setDecryptionPassphrase(this.existingPassphrase_)
        .then(
            sucessfullySet => this.handlePageStatusChanged_(
                sucessfullySet ? PageStatus.DONE :
                                 PageStatus.PASSPHRASE_FAILED));

    this.existingPassphrase_ = '';
  }

  private onPassphraseChanged_(e: CustomEvent<{didChange: boolean}>) {
    this.handlePageStatusChanged_(
        e.detail.didChange ? PageStatus.DONE : PageStatus.PASSPHRASE_FAILED);
  }

  /**
   * Called when the page status updates.
   */
  private handlePageStatusChanged_(pageStatus: PageStatus) {
    const router = Router.getInstance();
    switch (pageStatus) {
      case PageStatus.SPINNER:
      case PageStatus.CONFIGURE:
        this.pageStatus_ = pageStatus;
        return;
      case PageStatus.DONE:
        if (router.getCurrentRoute() === getSyncRoutes().SYNC) {
          router.navigateTo(getSyncRoutes().PEOPLE);
        }
        return;
      case PageStatus.PASSPHRASE_FAILED:
        if (this.pageStatus_ === PageStatus.CONFIGURE && this.syncPrefs &&
            this.syncPrefs.passphraseRequired) {
          const passphraseInput =
              this.shadowRoot!.querySelector<CrInputElement>(
                  '#existingPassphraseInput')!;
          passphraseInput.invalid = true;
          passphraseInput.focusInput();
        }
        return;
    }

    assertNotReached();
  }

  private onLearnMoreTap_(event: Event) {
    if ((event.target as HTMLElement).tagName === 'A') {
      // Stop the propagation of events, so that clicking on links inside
      // checkboxes or radio buttons won't change the value.
      event.stopPropagation();
    }
  }

  private shouldShowSyncAccountControl_(): boolean {
    // <if expr="chromeos">
    if (!loadTimeData.getBoolean('useBrowserSyncConsent')) {
      return false;
    }
    // </if>
    return this.syncStatus !== undefined &&
        !!this.syncStatus.syncSystemEnabled &&
        loadTimeData.getBoolean('signinAllowed');
  }

  private shouldShowExistingPassphraseBelowAccount_(): boolean {
    return this.syncPrefs !== undefined && !!this.syncPrefs.passphraseRequired;
  }

  private onSyncAdvancedClick_() {
    const router = Router.getInstance();
    router.navigateTo(this.getSyncAdvancedPageRoute_());
  }

  /**
   * @param e The event passed from settings-sync-account-control.
   */
  private onSyncSetupDone_(e: CustomEvent<boolean>) {
    if (e.detail) {
      this.didAbort_ = false;
      chrome.metricsPrivate.recordUserAction(
          'Signin_Signin_ConfirmAdvancedSyncSettings');
    } else {
      this.setupCancelConfirmed_ = true;
      chrome.metricsPrivate.recordUserAction(
          'Signin_Signin_CancelAdvancedSyncSettings');
    }
    const router = Router.getInstance();
    router.navigateTo(getSyncRoutes().BASIC);
  }

  /**
   * Focuses the passphrase input element if it is available and the page is
   * visible.
   */
  private focusPassphraseInput_() {
    const passphraseInput = this.shadowRoot!.querySelector<CrInputElement>(
        '#existingPassphraseInput');
    const router = Router.getInstance();
    if (passphraseInput && router.getCurrentRoute() === getSyncRoutes().SYNC) {
      passphraseInput.focus();
    }
  }
}

customElements.define(SettingsSyncPageElement.is, SettingsSyncPageElement);
