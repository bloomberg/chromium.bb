// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {isChromeOS} from '//resources/js/cr.m.js';
import {CrViewManagerElement} from 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {isRTL} from 'chrome://resources/js/util.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';

// <if expr="chromeos">
import './arc_account_picker_app.js';
import './gaia_action_buttons.js';
import './signin_blocked_by_policy_page.js';
import './welcome_page_app.js';
import './strings.m.js';

import {getAccountAdditionOptionsFromJSON} from './inline_login_util.js';
// </if>

import {AuthCompletedCredentials, Authenticator, AuthParams} from '../gaia_auth_host/authenticator.m.js';
import {Account, InlineLoginBrowserProxy, InlineLoginBrowserProxyImpl} from './inline_login_browser_proxy.js';

/**
 * @fileoverview Inline login WebUI in various signin flows for ChromeOS and
 * Chrome desktop (Windows only).
 */

/** @enum {string} */
const View = {
  addAccount: 'addAccount',
  signinBlockedByPolicy: 'signinBlockedByPolicy',
  welcome: 'welcome',
  arcAccountPicker: 'arcAccountPicker',
};

Polymer({
  is: 'inline-login-app',

  _template: html`{__html_template__}`,

  behaviors: [WebUIListenerBehavior, I18nBehavior],

  properties: {
    /** Mirroring the enum so that it can be used from HTML bindings. */
    View: {
      type: Object,
      value: View,
    },

    /**
     * Indicates whether the page is loading.
     * @private {boolean}
     */
    loading_: {
      type: Boolean,
      value: true,
    },

    /**
     * The auth extension host instance.
     * @private {?Authenticator}
     */
    authExtHost_: {
      type: Object,
      value: null,
    },

    // <if expr="chromeos">
    /*
     * True if welcome page should not be shown.
     * @private
     */
    shouldSkipWelcomePage_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('shouldSkipWelcomePage');
      },
      readOnly: true,
    },

    /*
     * True if `kArcAccountRestrictions` feature is enabled.
     * @private
     */
    isArcAccountRestrictionsEnabled_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('isArcAccountRestrictionsEnabled');
      },
      readOnly: true,
    },

    /*
     * True if the dialog is open for reauthentication.
     * @private
     */
    isReauthentication_: {
      type: Boolean,
      value: false,
    },

    /*
     * True if the account should be available in ARC++ after addition.
     * @private
     */
    isAvailableInArc_: {
      type: Boolean,
      value: false,
    },

    /**
     * Accounts which are not available in ARC and are shown on the ARC picker
     * screen.
     * @type {!Array<!Account>}
     */
    arcPickerAccounts_: {
      type: Array,
      value: [],
    },

    /**
     * User's email used in the sign-in flow.
     * @private {string}
     */
    email_: {type: String, value: ''},

    /**
     * Hosted domain of the user's email used in the sign-in flow.
     * @private {string}
     */
    hostedDomain_: {type: String, value: ''},
    // </if>

    /**
     * Id of the screen that is currently displayed.
     * @private {View}
     */
    currentView_: {
      type: String,
      value: '',
    },
  },

  /** @private {?InlineLoginBrowserProxy} */
  browserProxy_: null,

  /**
   * Whether the login UI is loaded for signing in primary account.
   * @private {boolean}
   */
  isLoginPrimaryAccount_: false,

  /**
   * TODO(crbug.com/1164862): cleanup this flag, since it's enabled by default.
   * @private {boolean}
   */
  enableGaiaActionButtons_: false,

  /** @override */
  created() {
    this.browserProxy_ = InlineLoginBrowserProxyImpl.getInstance();
  },

  /** @override */
  ready() {
    this.authExtHost_ = new Authenticator(
        /** @type {!WebView} */ (this.$.signinFrame));
    this.addAuthExtHostListeners_();
    this.browserProxy_.initialize();
  },

  /** @override */
  attached() {
    this.addWebUIListener(
        'load-auth-extension', data => this.loadAuthExtension_(data));
    this.addWebUIListener(
        'send-lst-fetch-results', arg => this.sendLSTFetchResults_(arg));
    this.addWebUIListener('close-dialog', () => this.closeDialog_());

    this.addWebUIListener(
        'show-signin-blocked-by-policy-page',
        data => this.signinBlockedByPolicyShowView_(data));
  },

  /** @private */
  addAuthExtHostListeners_() {
    this.authExtHost_.addEventListener(
        'dropLink',
        e => this.onDropLink_(
            /** @type {!CustomEvent<string>} */ (e)));
    this.authExtHost_.addEventListener(
        'newWindow',
        e => this.onNewWindow_(
            /** @type {!CustomEvent<NewWindowProperties>} */ (e)));
    this.authExtHost_.addEventListener('ready', () => this.onAuthReady_());
    this.authExtHost_.addEventListener(
        'resize',
        e => this.onResize_(
            /** @type {!CustomEvent<string>} */ (e)));
    this.authExtHost_.addEventListener(
        'authCompleted',
        e => this.onAuthCompleted_(
            /** @type {!CustomEvent<!AuthCompletedCredentials>} */ (e)));
    this.authExtHost_.addEventListener(
        'showIncognito', () => this.onShowIncognito_());
    this.authExtHost_.addEventListener(
        'getAccounts', () => this.onGetAccounts_());
  },

  /**
   * @param {!CustomEvent<string>} e
   * @private
   */
  onDropLink_(e) {
    // Navigate to the dropped link.
    window.location.href = e.detail;
  },

  /**
   * @param {!CustomEvent<NewWindowProperties>} e
   * @private
   */
  onNewWindow_(e) {
    window.open(e.detail.targetUrl, '_blank');
    e.detail.window.discard();
    // <if expr="chromeos">
    // On Chrome OS this dialog is always-on-top, so we have to close it if
    // user opens a link in a new window.
    this.closeDialog_();
    // </if>
  },

  /** @private */
  onAuthReady_() {
    this.loading_ = false;
    if (this.isLoginPrimaryAccount_) {
      this.browserProxy_.recordAction('Signin_SigninPage_Shown');
    }
    this.browserProxy_.authExtensionReady();
  },

  /**
   * @param {!CustomEvent<string>} e
   * @private
   */
  onResize_(e) {
    this.browserProxy_.switchToFullTab(e.detail);
  },

  /**
   * @param {!CustomEvent<!AuthCompletedCredentials>} e
   * @private
   */
  onAuthCompleted_(e) {
    this.loading_ = true;
    /** @type {!AuthCompletedCredentials} */
    const credentials = e.detail;

    // <if expr="chromeos_ash">
    if (this.isArcAccountRestrictionsEnabled_ && !this.isReauthentication_) {
      credentials.isAvailableInArc = this.isAvailableInArc_;
    }
    // </if>

    this.browserProxy_.completeLogin(credentials);
  },

  /** @private */
  onShowIncognito_() {
    this.browserProxy_.showIncognito();
  },

  /** @private */
  onGetAccounts_() {
    this.browserProxy_.getAccounts().then(result => {
      this.authExtHost_.getAccountsResponse(result);
    });
  },

  /**
   * Loads auth extension.
   * @param {!AuthParams} data Parameters for auth extension.
   * @private
   */
  loadAuthExtension_(data) {
    this.authExtHost_.load(data.authMode, data);
    this.loading_ = true;
    this.isLoginPrimaryAccount_ = data.isLoginPrimaryAccount;
    this.enableGaiaActionButtons_ = data.enableGaiaActionButtons;
    // Skip welcome page for reauthentication.
    if (data.email) {
      this.isReauthentication_ = true;
    }
    this.switchToDefaultView_();
  },

  /**
   * Sends a message 'lstFetchResults'. This is a specific message sent when
   * the inline signin is loaded with reason kFetchLstOnly. Handlers of
   * this message would expect a single argument a base::Dictionary value that
   * contains the values fetched from the gaia sign in endpoint.
   * @param {string} arg The string representation of the json data returned by
   *    the sign in dialog after it has finished the sign in process.
   * @private
   */
  sendLSTFetchResults_(arg) {
    this.browserProxy_.lstFetchResults(arg);
  },

  /**
   * Closes the login dialog.
   * @private
   */
  closeDialog_() {
    this.browserProxy_.dialogClose();
  },

  /**
   * Navigates to the welcome screen.
   * @private
   */
  goToWelcomeScreen_() {
    this.switchView_(View.welcome);
  },

  /**
   * Navigates back in the web view if possible. Otherwise closes the dialog.
   * @private
   */
  handleGoBack_() {
    if (this.$.signinFrame.canGoBack()) {
      this.$.signinFrame.back();
      this.$.signinFrame.focus();
    } else if (this.isWelcomePageEnabled_()) {
      // Allow user go back to the welcome page, if it's enabled.
      this.switchView_(View.welcome);
    } else {
      this.closeDialog_();
    }
  },

  /**
   * @return {string}
   * @private
   */
  getBackButtonIcon_() {
    return isRTL() ? 'cr:chevron-right' : 'cr:chevron-left';
  },

  /**
   * @return {string}
   * @private
   */
  getNextButtonLabel_() {
    return this.currentView_ === View.signinBlockedByPolicy ||
            !this.isArcAccountRestrictionsEnabled_ ?
        this.i18n('ok') :
        this.i18n('nextButtonLabel');
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowBackButton_() {
    return this.currentView_ === View.addAccount;
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowOkButton_() {
    return this.currentView_ === View.welcome ||
        this.currentView_ === View.signinBlockedByPolicy;
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowGaiaButtons_() {
    return this.enableGaiaActionButtons_ &&
        this.currentView_ === View.addAccount;
  },

  /**
   * Navigates to the default view.
   * @private
   */
  switchToDefaultView_() {
    const view = this.getDefaultView_();

    // <if expr="chromeos_ash">
    if (this.isArcAccountRestrictionsEnabled_ &&
        view === View.arcAccountPicker) {
      this.browserProxy_.getAccountsNotAvailableInArc().then(result => {
        // If there are no accounts to show in the picker - go directly to
        // the welcome screen.
        if (result.length === 0) {
          this.switchView_(View.welcome);
          return;
        }
        this.set('arcPickerAccounts_', result);
        this.switchView_(view);
      });
      return;
    }
    // </if>

    this.switchView_(view);
  },

  /**
   * @return {View}
   * @private
   */
  getDefaultView_() {
    // TODO(https://crbug.com/1155041): simplify this when the file will be
    // split into CrOS and non-CrOS parts.
    if (!isChromeOS) {
      // On non-ChromeOS always show 'Add account'.
      return View.addAccount;
    }

    // <if expr="chromeos_ash">
    if (this.isReauthentication_) {
      return View.addAccount;
    }
    if (this.isArcAccountRestrictionsEnabled_) {
      const options = getAccountAdditionOptionsFromJSON(
          InlineLoginBrowserProxyImpl.getInstance().getDialogArguments());
      if (!!options && options.showArcAvailabilityPicker) {
        return View.arcAccountPicker;
      }
    }
    return this.shouldSkipWelcomePage_ ? View.addAccount :
                                         View.welcome;
    // </if>
  },

  /**
   * @param {View} id identifier of the view that should be shown.
   * @private
   */
  switchView_(id) {
    this.currentView_ = id;
    /** @type {CrViewManagerElement} */ (this.$.viewManager).switchView(id);
  },

  /**
   * @return {boolean}
   * @private
   */
  isWelcomePageEnabled_() {
    if (!isChromeOS) {
      return false;
    }
    return !this.shouldSkipWelcomePage_ && !this.isReauthentication_;
  },

  // <if expr="chromeos">

  /**
   * Shows the sign-in blocked by policy screen.
   * @param {{email:string, hostedDomain:string}} data parameters.
   * @private
   */
  signinBlockedByPolicyShowView_(data) {
    this.set('email_', data.email);
    this.set('hostedDomain_', data.hostedDomain);
    this.switchView_(View.signinBlockedByPolicy);
    this.setFocusToWebview_();
  },

  /** @private */
  onOkButtonClick_() {
    switch (this.currentView_) {
      case View.welcome:
        this.switchView_(View.addAccount);
        const skipChecked =
            /** @type {WelcomePageAppElement} */ (this.$$('welcome-page-app'))
                .isSkipCheckboxChecked();
        this.browserProxy_.skipWelcomePage(skipChecked);
        this.setFocusToWebview_();
        break;
      case View.signinBlockedByPolicy:
        this.closeDialog_();
        break;
    }
  },

  /** @private */
  setFocusToWebview_() {
    this.$.signinFrame.focus();
  },
  // </if>

  /** @param {Object} authExtHost */
  setAuthExtHostForTest(authExtHost) {
    this.authExtHost_ = /** @type {!Authenticator} */ (authExtHost);
    this.addAuthExtHostListeners_();
  },
});
