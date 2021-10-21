// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview
 * 'settings-sync-account-section' is the settings page containing sign-in
 * settings.
 */
import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/cr_button/cr_button.m.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import '//resources/cr_elements/icons.m.js';
import '//resources/cr_elements/shared_style_css.m.js';
import '//resources/cr_elements/shared_vars_css.m.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import './profile_info_browser_proxy.js';
import '../icons.js';
import '../prefs/prefs.js';
import '../settings_shared_css.js';

import {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {assert} from '//resources/js/assert.m.js';
import {WebUIListenerMixin} from '//resources/js/web_ui_listener_mixin.js';
import {html, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {PrefsMixin} from '../prefs/prefs_mixin.js';
import {Route, Router} from '../router.js';

import {StatusAction, StoredAccount, SyncBrowserProxy, SyncBrowserProxyImpl, SyncStatus} from './sync_browser_proxy.js';

export const MAX_SIGNIN_PROMO_IMPRESSION: number = 10;

interface RepeaterEvent extends CustomEvent {
  model: {
    item: StoredAccount,
  };
}

const SettingsSyncAccountControlElementBase =
    WebUIListenerMixin(PrefsMixin(PolymerElement));

class SettingsSyncAccountControlElement extends
    SettingsSyncAccountControlElementBase {
  static get is() {
    return 'settings-sync-account-control';
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
       * The current sync status, supplied by parent element.
       */
      syncStatus: Object,

      // String to be used as a title when the promo has an account.
      promoLabelWithAccount: String,

      // String to be used as title of the promo has no account.
      promoLabelWithNoAccount: String,

      // String to be used as a subtitle when the promo has an account.
      promoSecondaryLabelWithAccount: String,

      // String to be used as subtitle of the promo has no account.
      promoSecondaryLabelWithNoAccount: String,

      /**
       * Proxy variable for syncStatus.signedIn to shield observer from being
       * triggered multiple times whenever syncStatus changes.
       */
      signedIn_: {
        type: Boolean,
        computed: 'computeSignedIn_(syncStatus.signedIn)',
        observer: 'onSignedInChanged_',
      },

      storedAccounts_: Object,

      shownAccount_: Object,

      showingPromo: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      // This property should be set by the parent only and should not change
      // after the element is created.
      embeddedInSubpage: {
        type: Boolean,
        reflectToAttribute: true,
      },

      // This property should be set by the parent only and should not change
      // after the element is created.
      hideButtons: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      shouldShowAvatarRow_: {
        type: Boolean,
        value: false,
        computed: 'computeShouldShowAvatarRow_(storedAccounts_, syncStatus,' +
            'storedAccounts_.length, syncStatus.signedIn)',
        observer: 'onShouldShowAvatarRowChange_',
      },

      subLabel_: {
        type: String,
        computed: 'computeSubLabel_(promoSecondaryLabelWithAccount,' +
            'promoSecondaryLabelWithNoAccount, shownAccount_)',
      },

      showSetupButtons_: {
        type: Boolean,
        computed: 'computeShowSetupButtons_(' +
            'hideButtons, syncStatus.firstSetupInProgress)',
      },
    };
  }

  static get observers() {
    return [
      'onShownAccountShouldChange_(storedAccounts_, syncStatus)',
    ];
  }

  syncStatus: SyncStatus;
  promoLabelWithAccount: string;
  promoLabelWithNoAccount: string;
  promoSecondaryLabelWithAccount: string;
  promoSecondaryLabelWithNoAccount: string;
  signedIn_: boolean;
  storedAccounts_: Array<StoredAccount>;
  private shownAccount_: StoredAccount|null;
  showingPromo: boolean;
  embeddedInSubpage: boolean;
  hideButtons: boolean;
  private shouldShowAvatarRow_: boolean;
  private subLabel_: string;
  private showSetupButtons_: boolean;
  private syncBrowserProxy_: SyncBrowserProxy =
      SyncBrowserProxyImpl.getInstance();

  connectedCallback() {
    super.connectedCallback();

    this.syncBrowserProxy_.getStoredAccounts().then(
        this.handleStoredAccounts_.bind(this));
    this.addWebUIListener(
        'stored-accounts-updated', this.handleStoredAccounts_.bind(this));
  }

  /**
   * Records the following user actions:
   * - Signin_Impression_FromSettings and
   * - Signin_ImpressionWithAccount_FromSettings
   * - Signin_ImpressionWithNoAccount_FromSettings
   */
  recordImpressionUserActions_() {
    assert(!this.syncStatus.signedIn);
    assert(this.shownAccount_ !== undefined);

    chrome.metricsPrivate.recordUserAction('Signin_Impression_FromSettings');
    if (this.shownAccount_) {
      chrome.metricsPrivate.recordUserAction(
          'Signin_ImpressionWithAccount_FromSettings');
    } else {
      chrome.metricsPrivate.recordUserAction(
          'Signin_ImpressionWithNoAccount_FromSettings');
    }
  }

  private computeSignedIn_(): boolean {
    return !!this.syncStatus && !!this.syncStatus.signedIn;
  }

  private onSignedInChanged_() {
    if (this.embeddedInSubpage) {
      this.showingPromo = true;
      return;
    }

    if (!this.showingPromo && !this.syncStatus.signedIn &&
        this.syncBrowserProxy_.getPromoImpressionCount() <
            MAX_SIGNIN_PROMO_IMPRESSION) {
      this.showingPromo = true;
      this.syncBrowserProxy_.incrementPromoImpressionCount();
    } else {
      // Turn off the promo if the user is signed in.
      this.showingPromo = false;
    }
    if (!this.syncStatus.signedIn && this.shownAccount_ !== undefined) {
      this.recordImpressionUserActions_();
    }
  }

  private getLabel_(labelWithAccount: string, labelWithNoAccount: string):
      string {
    return this.shownAccount_ ? labelWithAccount : labelWithNoAccount;
  }

  private computeSubLabel_(): string {
    return this.getLabel_(
        this.promoSecondaryLabelWithAccount,
        this.promoSecondaryLabelWithNoAccount);
  }

  private getSubstituteLabel_(label: string, name: string): string {
    return loadTimeData.substituteString(label, name);
  }

  private getAccountLabel_(label: string, account: string): string {
    if (this.syncStatus.firstSetupInProgress) {
      return this.syncStatus.statusText || account;
    }
    return this.syncStatus.signedIn && !this.syncStatus.hasError &&
            !this.syncStatus.disabled ?
        loadTimeData.substituteString(label, account) :
        account;
  }

  private getAccountImageSrc_(image: string|null): string {
    // image can be undefined if the account has not set an avatar photo.
    return image || 'chrome://theme/IDR_PROFILE_AVATAR_PLACEHOLDER_LARGE';
  }

  /**
   * @return The CSS class of the sync icon.
   */
  private getSyncIconStyle_(): string {
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
  }

  /**
   * Returned value must match one of iron-icon's settings:(*) icon name.
   */
  private getSyncIcon_(): string {
    switch (this.getSyncIconStyle_()) {
      case 'sync-problem':
        return 'settings:sync-problem';
      case 'sync-paused':
        return 'settings:sync-disabled';
      default:
        return 'cr:sync';
    }
  }

  private getAvatarRowTitle_(
      accountName: string, syncErrorLabel: string,
      syncPasswordsOnlyErrorLabel: string, authErrorLabel: string,
      disabledLabel: string): string {
    if (this.syncStatus.disabled) {
      return disabledLabel;
    }
    if (!this.syncStatus.hasError) {
      return accountName;
    }
    // Specific error cases below.
    if (this.syncStatus.hasUnrecoverableError) {
      return syncErrorLabel;
    }
    if (this.syncStatus.statusAction === StatusAction.REAUTHENTICATE) {
      return authErrorLabel;
    }
    if (this.syncStatus.hasPasswordsOnlyError) {
      return syncPasswordsOnlyErrorLabel;
    }
    return syncErrorLabel;
  }

  /**
   * Determines if the sync button should be disabled in response to
   * either a first setup flow or chrome sign-in being disabled.
   */
  private shouldDisableSyncButton_(): boolean {
    if (this.hideButtons || this.prefs === undefined) {
      return this.computeShowSetupButtons_();
    }
    return !!this.syncStatus.firstSetupInProgress ||
        !this.getPref('signin.allowed_on_next_startup').value;
  }

  private shouldShowTurnOffButton_(): boolean {
    // <if expr="chromeos">
    if (this.syncStatus.domain) {
      // Chrome OS cannot delete the user's profile like other platforms, so
      // hide the turn off sync button for enterprise users who are not
      // allowed to sign out.
      return false;
    }
    // </if>
    // <if expr="lacros">
    if (loadTimeData.getBoolean('isMainProfile')) {
      // On Lacros the primary account doesn't support turning off sync yet.
      // TODO(https://crbug.com/1217645): Remove after adding sync off state.
      return false;
    }
    // </if>
    return !this.hideButtons && !this.showSetupButtons_ &&
        !!this.syncStatus.signedIn;
  }

  private shouldShowErrorActionButton_(): boolean {
    if (this.embeddedInSubpage &&
        this.syncStatus.statusAction === StatusAction.ENTER_PASSPHRASE) {
      // In a subpage the passphrase button is not required.
      return false;
    }
    return !this.hideButtons && !this.showSetupButtons_ &&
        !!this.syncStatus.signedIn && !!this.syncStatus.hasError &&
        this.syncStatus.statusAction !== StatusAction.NO_ACTION;
  }

  private handleStoredAccounts_(accounts: Array<StoredAccount>) {
    this.storedAccounts_ = accounts;
  }

  private computeShouldShowAvatarRow_(): boolean {
    if (this.storedAccounts_ === undefined || this.syncStatus === undefined) {
      return false;
    }

    return this.syncStatus.signedIn || this.storedAccounts_.length > 0;
  }

  private onErrorButtonTap_() {
    const router = Router.getInstance();
    const routes =
        router.getRoutes() as {SIGN_OUT: Route, SYNC: Route, ABOUT: Route};
    switch (this.syncStatus.statusAction) {
      // <if expr="not chromeos">
      case StatusAction.REAUTHENTICATE:
        this.syncBrowserProxy_.startSignIn();
        break;
      // </if>
      case StatusAction.UPGRADE_CLIENT:
        router.navigateTo(routes.ABOUT);
        break;
      case StatusAction.RETRIEVE_TRUSTED_VAULT_KEYS:
        this.syncBrowserProxy_.startKeyRetrieval();
        break;
      case StatusAction.ENTER_PASSPHRASE:
      case StatusAction.CONFIRM_SYNC_SETTINGS:
      default:
        router.navigateTo(routes.SYNC);
    }
  }

  private onSigninTap_() {
    // <if expr="not chromeos">
    this.syncBrowserProxy_.startSignIn();
    // </if>
    // <if expr="chromeos">
    // Chrome OS is always signed-in, so just turn on sync.
    this.syncBrowserProxy_.turnOnSync();
    // </if>
    // Need to close here since one menu item also triggers this function.
    const actionMenu = this.shadowRoot!.querySelector('cr-action-menu');
    if (actionMenu) {
      actionMenu.close();
    }
  }

  // <if expr="not chromeos">
  private onSignoutTap_() {
    this.syncBrowserProxy_.signOut(false /* deleteProfile */);
    this.shadowRoot!.querySelector('cr-action-menu')!.close();
  }
  // </if>

  private onSyncButtonTap_() {
    assert(this.shownAccount_);
    assert(this.storedAccounts_.length > 0);
    const isDefaultPromoAccount =
        (this.shownAccount_!.email === this.storedAccounts_[0].email);

    this.syncBrowserProxy_.startSyncingWithEmail(
        this.shownAccount_!.email, isDefaultPromoAccount);
  }

  private onTurnOffButtonTap_() {
    /* This will route to people_page's disconnect dialog. */
    const router = Router.getInstance();
    router.navigateTo((router.getRoutes() as {SIGN_OUT: Route}).SIGN_OUT);
  }

  private onMenuButtonTap_() {
    const actionMenu = this.shadowRoot!.querySelector('cr-action-menu')!;
    actionMenu.showAt(
        assert(this.shadowRoot!.querySelector('#dropdown-arrow')!));
  }

  private onShouldShowAvatarRowChange_() {
    // Close dropdown when avatar-row hides, so if it appears again, the menu
    // won't be open by default.
    const actionMenu = this.shadowRoot!.querySelector('cr-action-menu');
    if (!this.shouldShowAvatarRow_ && actionMenu && actionMenu.open) {
      actionMenu.close();
    }
  }

  private onAccountTap_(e: RepeaterEvent) {
    this.shownAccount_ = e.model.item;
    this.shadowRoot!.querySelector('cr-action-menu')!.close();
  }

  private onShownAccountShouldChange_() {
    if (this.storedAccounts_ === undefined || this.syncStatus === undefined) {
      return;
    }

    if (this.syncStatus.signedIn) {
      for (let i = 0; i < this.storedAccounts_.length; i++) {
        if (this.storedAccounts_[i].email ===
            this.syncStatus.signedInUsername) {
          this.shownAccount_ = this.storedAccounts_[i];
          return;
        }
      }
    } else {
      const firstStoredAccount =
          (this.storedAccounts_.length > 0) ? this.storedAccounts_[0] : null;

      // Sign-in impressions should be recorded in the following cases:
      // 1. When the promo is first shown, i.e. when |shownAccount_| is
      //   initialized;
      // 2. When the impression account state changes, i.e. promo impression
      //   state changes (WithAccount -> WithNoAccount) or
      //   (WithNoAccount -> WithAccount).
      const shouldRecordImpression = (this.shownAccount_ === undefined) ||
          (!this.shownAccount_ && firstStoredAccount) ||
          (this.shownAccount_ && !firstStoredAccount);

      this.shownAccount_ = firstStoredAccount;

      if (shouldRecordImpression) {
        this.recordImpressionUserActions_();
      }
    }
  }

  private computeShowSetupButtons_(): boolean {
    return !this.hideButtons && !!this.syncStatus.firstSetupInProgress;
  }

  private onSetupCancel_() {
    this.dispatchEvent(new CustomEvent(
        'sync-setup-done', {bubbles: true, composed: true, detail: false}));
  }

  private onSetupConfirm_() {
    this.dispatchEvent(new CustomEvent(
        'sync-setup-done', {bubbles: true, composed: true, detail: true}));
  }
}

customElements.define(
    SettingsSyncAccountControlElement.is, SettingsSyncAccountControlElement);
