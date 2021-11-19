// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview PasswordCheckListItem represents one insecure credential in the
 * list of insecure passwords.
 */

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/js/action_link.js';
import '../settings_shared_css.js';
import '../site_favicon.js';
import './passwords_shared_css.js';

import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {OpenWindowProxyImpl} from '../open_window_proxy.js';

// <if expr="chromeos or lacros">
import {BlockingRequestManager} from './blocking_request_manager.js';
// </if>
import {PasswordCheckInteraction, PasswordManagerImpl, PasswordManagerProxy} from './password_manager_proxy.js';


export class PasswordCheckListItemElement extends PolymerElement {
  static get is() {
    return 'password-check-list-item';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      // <if expr="chromeos or lacros">
      tokenRequestManager: Object,
      // </if>

      /**
       * The password that is being displayed.
       */
      item: Object,

      isPasswordVisible: {
        type: Boolean,
        computed: 'computePasswordVisibility_(item.password)',
      },

      password_: {
        type: String,
        computed: 'computePassword_(item.password)',
      },

      clickedChangePassword: {
        type: Boolean,
        value: false,
      },

      buttonClass_: {
        type: String,
        computed: 'computeButtonClass_(item.compromisedInfo)',
      },

      iconClass_: {
        type: String,
        computed: 'computeIconClass_(item.compromisedInfo)',
      },
    };
  }

  // <if expr="chromeos or lacros">
  tokenRequestManager: BlockingRequestManager;
  // </if>

  item: chrome.passwordsPrivate.InsecureCredential;
  isPasswordVisible: boolean;
  private password_: string;
  clickedChangePassword: boolean;
  private buttonClass_: string;
  private iconClass_: string;
  private passwordManager_: PasswordManagerProxy =
      PasswordManagerImpl.getInstance();

  /**
   * @return Whether |item| is compromised credential.
   */
  private isCompromisedItem_(): boolean {
    return !!this.item.compromisedInfo;
  }

  private getCompromiseType_(): string {
    switch (this.item.compromisedInfo!.compromiseType) {
      case chrome.passwordsPrivate.CompromiseType.PHISHED:
        return loadTimeData.getString('phishedPassword');
      case chrome.passwordsPrivate.CompromiseType.LEAKED:
        return loadTimeData.getString('leakedPassword');
      case chrome.passwordsPrivate.CompromiseType.PHISHED_AND_LEAKED:
        return loadTimeData.getString('phishedAndLeakedPassword');
    }

    assertNotReached(
        'Can\'t find a string for type: ' +
        this.item.compromisedInfo!.compromiseType);
    return '';
  }

  private fire_(eventName: string, detail?: any) {
    this.dispatchEvent(
        new CustomEvent(eventName, {bubbles: true, composed: true, detail}));
  }

  private onChangePasswordClick_() {
    this.fire_('change-password-clicked', {id: this.item.id});

    const url = assert(this.item.changePasswordUrl!);
    OpenWindowProxyImpl.getInstance().openURL(url);

    PasswordManagerImpl.getInstance().recordPasswordCheckInteraction(
        PasswordCheckInteraction.CHANGE_PASSWORD);
  }

  private onMoreClick_(event: Event) {
    this.fire_('more-actions-click', {moreActionsButton: event.target});
  }

  private getInputType_(): string {
    return this.isPasswordVisible ? 'text' : 'password';
  }

  private computePasswordVisibility_(): boolean {
    return !!this.item.password;
  }

  private computeButtonClass_(): string {
    if (this.item.compromisedInfo) {
      // Strong CTA.
      return 'action-button';
    }
    // Weak CTA.
    return '';
  }

  private computeIconClass_(): string {
    if (this.item.compromisedInfo) {
      // Strong CTA, white icon.
      return '';
    }
    // Weak CTA, non-white-icon.
    return 'icon-weak-cta';
  }

  private computePassword_(): string {
    const NUM_PLACEHOLDERS = 10;
    return this.item.password || ' '.repeat(NUM_PLACEHOLDERS);
  }

  hidePassword() {
    this.set('item.password', null);
  }

  showPassword() {
    this.passwordManager_.recordPasswordCheckInteraction(
        PasswordCheckInteraction.SHOW_PASSWORD);
    this.passwordManager_
        .getPlaintextInsecurePassword(
            assert(this.item), chrome.passwordsPrivate.PlaintextReason.VIEW)
        .then(
            insecureCredential => {
              this.set('item', insecureCredential);
            },
            _error => {
              // <if expr="chromeos or lacros">
              // If no password was found, refresh auth token and retry.
              this.tokenRequestManager.request(() => this.showPassword());
              // </if>
            });
  }

  private onReadonlyInputTap_() {
    if (this.isPasswordVisible) {
      (this.shadowRoot!.querySelector('#leakedPassword') as HTMLInputElement)
          .select();
    }
  }

  private onAlreadyChangedClick_(event: Event) {
    event.preventDefault();
    this.fire_('already-changed-password-click', event.target);
  }
}

customElements.define(
    PasswordCheckListItemElement.is, PasswordCheckListItemElement);
