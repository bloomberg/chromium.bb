// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../account_manager_shared_css.js';

import {getImage} from '//resources/js/icon.js';
import {html, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {Account, ArcAccountPickerBrowserProxy, ArcAccountPickerBrowserProxyImpl} from './arc_account_picker_browser_proxy.js';

/**
 * @fileoverview Polymer element for showing a list of accounts that are not
 * available in ARC and allowing user to make them available in ARC. Used by
 * UIs in inline login dialog.
 *
 * Usage:
 * $$('arc-account-picker-app').loadAccounts().then(accountsFound => {
 *    if (accountsFound) {
 *      // show the arc-account-picker-app
 *    } else {
 *      // no accounts found
 *    }
 * });
 */

/** @typedef {{model: !{item: Account}, target: !Element}} */
let EventModel;

/** @polymer */
export class ArcAccountPickerAppElement extends PolymerElement {
  static get is() {
    return 'arc-account-picker-app';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Accounts which are not available in ARC and are shown on the ARC picker
       * screen.
       * @private {!Array<!Account>}
       */
      accounts_: {
        type: Array,
      }
    };
  }

  constructor() {
    super();

    /** @private {!ArcAccountPickerBrowserProxy} */
    this.browserProxy_ = ArcAccountPickerBrowserProxyImpl.getInstance();
  }

  ready() {
    super.ready();

    this.shadowRoot.querySelector('#osSettingsLink')
        .addEventListener(
            'click',
            () => this.dispatchEvent(new CustomEvent('opened-new-window')));
  }

  /**
   * Call `getAccountsNotAvailableInArc` to load accounts that are not available
   * in ARC.
   * @return {!Promise<boolean>} a promise of boolean, if the value is true -
   *     there are > 0 accounts found, false otherwise.
   */
  loadAccounts() {
    return new Promise((resolve) => {
      this.browserProxy_.getAccountsNotAvailableInArc().then(result => {
        if (result.length === 0) {
          resolve(false);
        }
        this.set('accounts_', result);
        resolve(true);
      });
    });
  }

  /**
   * @param {string} iconUrl
   * @return {string} A CSS image-set for multiple scale factors.
   * @private
   */
  getIconImageSet_(iconUrl) {
    return getImage(iconUrl);
  }

  /**
   * Navigates to the welcome screen.
   * @private
   */
  addAccount_() {
    this.dispatchEvent(new CustomEvent('add-account'));
  }

  /**
   * @param {!EventModel} event
   * @private
   */
  makeAvailableInArc_(event) {
    this.browserProxy_.makeAvailableInArc(event.model.item);
  }

  /**
   * @param {!KeyboardEvent} e
   * @private
   */
  onAddAccountKeyPress_(e) {
    if (e.key === 'Space' || e.key === 'Enter') {
      e.stopPropagation();
      this.addAccount_();
    }
  }

  /**
   * @param {!KeyboardEvent} e
   * @private
   */
  onAccountKeyPress_(e) {
    if (e.key === 'Space' || e.key === 'Enter') {
      e.stopPropagation();
      this.makeAvailableInArc_(
          /** @type {!EventModel} */ (e));
    }
  }
}

customElements.define(
    ArcAccountPickerAppElement.is, ArcAccountPickerAppElement);
