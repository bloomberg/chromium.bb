// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'password-move-multiple-passwords-to-account-dialog' is the
 * dialog that allows moving multiple passwords stored on the user device to the
 * account.
 */

import './password_list_item.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MultiStorePasswordUiEntry} from './multi_store_password_ui_entry.js';
import {PasswordManagerImpl} from './password_manager_proxy.js';
import {MoveToAccountStoreTrigger} from './password_move_to_account_dialog.js';

/** @polymer */
class PasswordMoveMultiplePasswordsToAccountDialogElement extends
    PolymerElement {
  static get is() {
    return 'password-move-multiple-passwords-to-account-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {!Array<!MultiStorePasswordUiEntry>} */
      passwordsToMove: {
        type: Array,
        value: () => [],
      },

      accountEmail: String,
    };
  }

  /** @return {boolean} Whether the user confirmed the dialog. */
  wasConfirmed() {
    return this.$.dialog.getNative().returnValue === 'success';
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    chrome.metricsPrivate.recordEnumerationValue(
        'PasswordManager.AccountStorage.MoveToAccountStoreFlowOffered',
        MoveToAccountStoreTrigger
            .EXPLICITLY_TRIGGERED_FOR_MULTIPLE_PASSWORDS_IN_SETTINGS,
        MoveToAccountStoreTrigger.COUNT);
  }

  /** @private */
  onMoveButtonClick_() {
    const checkboxes = this.$.dialog.querySelectorAll('cr-checkbox');
    const selectedPasswords = [];
    checkboxes.forEach((checkbox) => {
      if (checkbox.checked) {
        selectedPasswords.push(Number(checkbox.dataset.id));
      }
    });
    PasswordManagerImpl.getInstance().movePasswordsToAccount(selectedPasswords);
    chrome.metricsPrivate.recordSmallCount(
        'PasswordManager.AccountStorage.MoveToAccountStorePasswordsCount',
        selectedPasswords.length);
    this.$.dialog.close();
  }

  /** @private */
  onCancelButtonClick_() {
    this.$.dialog.cancel();
  }
}

customElements.define(
    PasswordMoveMultiplePasswordsToAccountDialogElement.is,
    PasswordMoveMultiplePasswordsToAccountDialogElement);
