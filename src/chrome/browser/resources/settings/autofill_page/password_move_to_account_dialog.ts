// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'password-move-to-account-dialog' is the dialog that allows
 * moving a password stored on the user device to the account.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import './avatar_icon.js';
import '../site_favicon.js';

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MultiStorePasswordUiEntry} from './multi_store_password_ui_entry.js';
import {PasswordManagerImpl} from './password_manager_proxy.js';

/**
 * This should be kept in sync with the enum in
 * components/password_manager/core/browser/password_manager_metrics_util.h.
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 */
export enum MoveToAccountStoreTrigger {
  SUCCESSFUL_LOGIN_WITH_PROFILE_STORE_PASSWORD = 0,
  EXPLICITLY_TRIGGERED_IN_SETTINGS = 1,
  EXPLICITLY_TRIGGERED_FOR_MULTIPLE_PASSWORDS_IN_SETTINGS = 2,
  USER_OPTED_IN_AFTER_SAVING_LOCALLY = 3,
  COUNT = 4,
}

interface PasswordMoveToAccountDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}

class PasswordMoveToAccountDialogElement extends PolymerElement {
  static get is() {
    return 'password-move-to-account-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      passwordToMove: Object,
    };
  }

  passwordToMove: MultiStorePasswordUiEntry;

  connectedCallback() {
    super.connectedCallback();

    chrome.send('metricsHandler:recordInHistogram', [
      'PasswordManager.AccountStorage.MoveToAccountStoreFlowOffered',
      MoveToAccountStoreTrigger.EXPLICITLY_TRIGGERED_IN_SETTINGS,
      MoveToAccountStoreTrigger.COUNT,
    ]);

    this.$.dialog.showModal();
  }

  private onMoveButtonClick_() {
    assert(this.passwordToMove.isPresentOnDevice());
    PasswordManagerImpl.getInstance().movePasswordsToAccount(
        [this.passwordToMove.deviceId!]);
    this.$.dialog.close();
  }

  private onCancelButtonClick_() {
    this.$.dialog.close();
  }
}

customElements.define(
    PasswordMoveToAccountDialogElement.is, PasswordMoveToAccountDialogElement);
