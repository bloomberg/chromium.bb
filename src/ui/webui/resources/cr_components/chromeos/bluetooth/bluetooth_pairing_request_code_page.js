// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * UI element shown when authentication via PIN or PASSKEY is required
 * during Bluetooth device pairing.
 */

import './bluetooth_base_page.js';
import '../../../cr_elements/shared_style_css.m.js';
import '../../../cr_elements/cr_input/cr_input.m.js';

import {I18nBehavior, I18nBehaviorInterface} from '//resources/js/i18n_behavior.m.js';
import {afterNextRender, html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ButtonBarState, ButtonState, PairingAuthType} from './bluetooth_types.js';
import {mojoString16ToString} from './bluetooth_utils.js';


/**
 * Maximum length of a PIN code, it can range from 1 to 6 digits.
 * @type {number}
 */
const PIN_CODE_MAX_LENGTH = 6;

/**
 * Maximum length of a passkey, it can range from 1 to 16 characters.
 * @type {number}
 */
const PASSKEY_MAX_LENGTH = 16;

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const SettingsBluetoothPairingRequestCodePageElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class SettingsBluetoothRequestCodePageElement extends
    SettingsBluetoothPairingRequestCodePageElementBase {
  static get is() {
    return 'bluetooth-pairing-request-code-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * @type {?chromeos.bluetoothConfig.mojom.BluetoothDeviceProperties}
       */
      device: {
        type: Object,
        value: null,
      },

      /** @type {?PairingAuthType} */
      authType: {
        type: Object,
        value: null,
      },

      /** @private {!ButtonBarState} */
      buttonBarState_: {
        type: Object,
        computed: 'computeButtonBarState_(pinCode_)',
      },

      /** @private {string} */
      pinCode_: {
        type: String,
        value: '',
      }
    };
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    afterNextRender(this, () => {
      this.$.pin.focus();
    });
  }

  /**
   * @private
   * @return {string}
   */
  getMessage_() {
    return this.i18n('bluetoothEnterPin', this.getDeviceName_());
  }

  /**
   * @private
   * @return {string}
   */
  getDeviceName_() {
    if (!this.device) {
      return '';
    }

    return mojoString16ToString(this.device.publicName);
  }

  /**
   * @return {!ButtonBarState}
   * @private
   */
  computeButtonBarState_() {
    const pairButtonState =
        !this.pinCode_ ? ButtonState.DISABLED : ButtonState.ENABLED;

    return {
      cancel: ButtonState.ENABLED,
      pair: pairButtonState,
    };
  }

  /**
   * @param {!Event} event
   * @private
   */
  onPairClicked_(event) {
    event.stopPropagation();

    // TODO(crbug.com/1010321): Show spinner while pairing.
    if (!this.pinCode_) {
      return;
    }

    this.dispatchEvent(new CustomEvent('request-code-entered', {
      bubbles: true,
      composed: true,
      detail: {code: this.pinCode_},
    }));
  }

  /**
   * @private
   * @return {number}
   */
  getMaxlength_() {
    if (this.authType === PairingAuthType.REQUEST_PIN_CODE) {
      return PIN_CODE_MAX_LENGTH;
    }

    return PASSKEY_MAX_LENGTH;
  }
}

customElements.define(
    SettingsBluetoothRequestCodePageElement.is,
    SettingsBluetoothRequestCodePageElement);
