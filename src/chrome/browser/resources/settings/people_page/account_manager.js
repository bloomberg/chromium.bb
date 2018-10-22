// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-account-manager' is the settings subpage containing controls to
 * list, add and delete Secondary Google Accounts.
 */

Polymer({
  is: 'settings-account-manager',

  behaviors: [
    I18nBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    /**
     * List of Accounts.
     * @type {!Array<settings.Account>}
     */
    accounts_: {
      type: Array,
      value: function() {
        return [];
      },
    },

    /**
     * The targeted account for menu operations.
     * @private {?settings.Account}
     */
    actionMenuAccount_: Object,
  },

  /** @private {?settings.AccountManagerBrowserProxy} */
  browserProxy_: null,

  /** @override */
  attached: function() {
    this.addWebUIListener('accounts-changed', this.refreshAccounts_.bind(this));
  },

  /** @override */
  ready: function() {
    this.browserProxy_ = settings.AccountManagerBrowserProxyImpl.getInstance();
    this.refreshAccounts_();
  },

  /**
   * @param {string} iconUrl
   * @return {string} A CSS image-set for multiple scale factors.
   * @private
   */
  getIconImageSet_: function(iconUrl) {
    return cr.icon.getImage(iconUrl);
  },

  /**
   * @param {!Event} event
   * @private
   */
  addAccount_: function(event) {
    this.browserProxy_.addAccount();
  },

  /**
   * @private
   */
  refreshAccounts_: function() {
    this.browserProxy_.getAccounts().then(accounts => {
      this.set('accounts_', accounts);
    });
  },

  /**
   * Opens the Account actions menu.
   * @param {!{model: !{item: settings.Account}, target: !Element}} event
   * @private
   */
  onAccountActionsMenuButtonTap_: function(event) {
    this.actionMenuAccount_ = event.model.item;
    /** @type {!CrActionMenuElement} */ (this.$$('cr-action-menu'))
        .showAt(event.target);
  },

  /**
   * Closes action menu and resets action menu model.
   * @private
   */
  closeActionMenu_: function() {
    this.$$('cr-action-menu').close();
    this.actionMenuAccount_ = null;
  },

  /**
   * Removes the account being pointed to by |this.actionMenuAccount_|.
   * @private
   */
  onRemoveAccountTap_: function() {
    this.browserProxy_.removeAccount(
        /** @type {?settings.Account} */ (this.actionMenuAccount_));
    this.closeActionMenu_();
  }
});
