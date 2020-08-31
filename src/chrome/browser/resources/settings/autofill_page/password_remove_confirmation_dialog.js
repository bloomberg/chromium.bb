// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import '../i18n_setup.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PasswordManagerImpl, PasswordManagerProxy} from './password_manager_proxy.js';

Polymer({
  is: 'settings-password-remove-confirmation-dialog',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  properties: {
    /**
     * The password that is being displayed.
     * @private {?PasswordManagerProxy.CompromisedCredential}
     */
    item: Object,

  },

  /** @private {PasswordManagerProxy} */
  passwordManager_: null,

  /** @override */
  attached() {
    // Set the manager. These can be overridden by tests.
    this.passwordManager_ = PasswordManagerImpl.getInstance();
    this.$.dialog.showModal();
  },

  /** @private */
  onRemoveClick_() {
    this.passwordManager_.recordPasswordCheckInteraction(
        PasswordManagerProxy.PasswordCheckInteraction.REMOVE_PASSWORD);
    this.passwordManager_.removeCompromisedCredential(assert(this.item));
    this.$.dialog.close();
  },

  /** @private */
  onCancelClick_() {
    this.$.dialog.close();
  },

  /**
   * @return {string}
   * @private
   */
  getRemovePasswordDescription_() {
    return this.i18n(
        'removeCompromisedPasswordConfirmationDescription',
        this.item.formattedOrigin, this.item.formattedOrigin);
  }
});
