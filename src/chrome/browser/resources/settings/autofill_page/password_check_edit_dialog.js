// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'password-check-edit-dialog' is the dialog that allows showing
 * a saved password.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import '../icons.js';
import '../settings_shared_css.js';
import '../settings_vars_css.js';
import './passwords_shared_css.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {focusWithoutInk} from 'chrome://resources/js/cr/ui/focus_without_ink.m.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PasswordManagerImpl, PasswordManagerProxy} from './password_manager_proxy.js';



/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const SettingsPasswordCheckEditDialogElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
class SettingsPasswordCheckEditDialogElement extends
    SettingsPasswordCheckEditDialogElementBase {
  static get is() {
    return 'settings-password-check-edit-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * The password that the user is interacting with now.
       * @type {?PasswordManagerProxy.InsecureCredential}
       */
      item: Object,

      /**
       * Whether the password is visible or obfuscated.
       * @private
       */
      visible: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether the input is invalid.
       * @private
       */
      inputInvalid_: Boolean,

    };
  }

  constructor() {
    super();

    /** @private {!PasswordManagerProxy} */
    this.passwordManager_ = PasswordManagerImpl.getInstance();
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    this.$.dialog.showModal();
    focusWithoutInk(this.$.cancel);
  }

  /** Closes the dialog. */
  close() {
    this.$.dialog.close();
  }

  /**
   * Handler for tapping the 'cancel' button. Should just dismiss the dialog.
   * @private
   */
  onCancel_() {
    this.close();
  }

  /**
   * Handler for tapping the 'save' button. Should just dismiss the dialog.
   * @private
   */
  onSave_() {
    this.passwordManager_.recordPasswordCheckInteraction(
        PasswordManagerProxy.PasswordCheckInteraction.EDIT_PASSWORD);
    this.passwordManager_
        .changeInsecureCredential(assert(this.item), this.$.passwordInput.value)
        .finally(() => {
          this.close();
        });
  }

  /**
   * @private
   * @return {string} The title text for the show/hide icon.
   */
  showPasswordTitle_() {
    return this.i18n(this.visible ? 'hidePassword' : 'showPassword');
  }

  /**
   * @private
   * @return {string} The visibility icon class, depending on whether the
   *     password is already visible.
   */
  showPasswordIcon_() {
    return this.visible ? 'icon-visibility-off' : 'icon-visibility';
  }

  /**
   * @private
   * @return {string} The type of the password input field (text or password),
   *     depending on whether the password should be obfuscated.
   */
  getPasswordInputType_() {
    return this.visible ? 'text' : 'password';
  }

  /**
   * Handler for tapping the show/hide button.
   * @private
   */
  onShowPasswordButtonClick_() {
    this.visible = !this.visible;
  }

  /**
   * @private
   * @return {string} The text to be displayed as the dialog's footnote.
   */
  getFootnote_() {
    return this.i18n('editPasswordFootnote', this.item.formattedOrigin);
  }

  /**
   * @private
   * @return {string} The label for the origin, depending on the whether it's a
   *     site or an app.
   */
  getSiteOrApp_() {
    return this.i18n(
        this.item.isAndroidCredential ? 'editCompromisedPasswordApp' :
                                        'editCompromisedPasswordSite');
  }
}

customElements.define(
    SettingsPasswordCheckEditDialogElement.is,
    SettingsPasswordCheckEditDialogElement);
