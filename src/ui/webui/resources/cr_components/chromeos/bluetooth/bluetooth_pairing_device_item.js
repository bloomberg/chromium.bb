// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * UI element to show a list of discovered Bluetooth devices and initiate
 * pairing to a device.
 */
import '../../../cr_elements/shared_style_css.m.js';
import './bluetooth_icon.js';

import {I18nBehavior, I18nBehaviorInterface} from '//resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {FocusRowBehavior} from 'chrome://resources/js/cr/ui/focus_row_behavior.m.js';
import {BluetoothDeviceProperties, DeviceType} from 'chrome://resources/mojo/chromeos/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';

import {assertNotReached} from '../../../js/assert.m.js';

import {DeviceItemState} from './bluetooth_types.js';
import {mojoString16ToString} from './bluetooth_utils.js';

/**
 * @constructor
 * @implements {I18nBehaviorInterface}
 * @extends {PolymerElement}
 */
const SettingsBluetoothPairingDeviceItemElementBase =
    mixinBehaviors([I18nBehavior, FocusRowBehavior], PolymerElement);

/** @polymer */
export class SettingsBluetoothPairingDeviceItemElement extends
    SettingsBluetoothPairingDeviceItemElementBase {
  static get is() {
    return 'bluetooth-pairing-device-item';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * @type {?BluetoothDeviceProperties}
       */
      device: Object,

      /** @type {DeviceItemState} */
      deviceItemState: {
        type: Object,
        value: DeviceItemState.DEFAULT,
      },

      /** The index of this item in its parent list, used for its a11y label. */
      itemIndex: Number,

      /**
       * The total number of elements in this item's parent list, used for its
       * a11y label.
       */
      listSize: Number,

      /** @private {string} */
      secondaryLabel_: {
        type: String,
        computed: 'computeSecondaryLabel_(deviceItemState)',
      },

      /** @private {boolean} */
      pairingFailed_: {
        reflectToAttribute: true,
        type: Boolean,
        computed: 'computePairingFailed_(deviceItemState)',
      },
    };
  }

  /** @override */
  focus() {
    // Prevent scroll stops iron list from trying to bring this element to view,
    // if it is the |lastFocused| element and scrolled out of view. This can
    // happen if this element is tabbed to or selected and then scrolled out of
    // view.
    // TODO(b/210743107) Add a test for this.
    this.$.container.focus({preventScroll: true});
  }

  /**
   * @return {boolean}
   * @private
   */
  computePairingFailed_() {
    return this.deviceItemState === DeviceItemState.FAILED;
  }

  /**
   * @return {string}
   * @private
   */
  getDeviceName_() {
    if (!this.device) {
      return '';
    }
    return mojoString16ToString(this.device.publicName);
  }

  /**
   * @param {!Event} event
   * @private
   */
  onSelected_(event) {
    this.dispatchPairDeviceEvent_();
    event.stopPropagation();
  }

  /**
   * @param {!KeyboardEvent} event
   * @private
   */
  onKeydown_(event) {
    if (event.key !== 'Enter' && event.key !== ' ') {
      return;
    }

    this.dispatchPairDeviceEvent_();
    event.stopPropagation();
  }

  /**
   * @return {string}
   * @private
   */
  computeSecondaryLabel_() {
    switch (this.deviceItemState) {
      case DeviceItemState.FAILED:
        return this.i18n('bluetoothPairingFailed');
      case DeviceItemState.PAIRING:
        return this.i18n('bluetoothPairing');
      case DeviceItemState.DEFAULT:
        return '';
      default:
        assertNotReached();
    }
  }

  /** @private */
  dispatchPairDeviceEvent_() {
    this.dispatchEvent(new CustomEvent('pair-device', {
      bubbles: true,
      composed: true,
      detail: {device: this.device},
    }));
  }

  /**
   * @return {string}
   * @private
   */
  getAriaLabel_() {
    if (!this.device) {
      return '';
    }

    return this.i18n(
               'bluetoothA11yDeviceName', this.itemIndex + 1, this.listSize,
               this.getDeviceName_()) +
        ' ' + this.i18n(this.getA11yDeviceTypeTextName_());
  }

  /**
   * @return {string}
   * @private
   */
  getA11yDeviceTypeTextName_() {
    switch (this.device.deviceType) {
      case DeviceType.kUnknown:
        return 'bluetoothA11yDeviceTypeUnknown';
      case DeviceType.kComputer:
        return 'bluetoothA11yDeviceTypeComputer';
      case DeviceType.kPhone:
        return 'bluetoothA11yDeviceTypePhone';
      case DeviceType.kHeadset:
        return 'bluetoothA11yDeviceTypeHeadset';
      case DeviceType.kVideoCamera:
        return 'bluetoothA11yDeviceTypeVideoCamera';
      case DeviceType.kGameController:
        return 'bluetoothA11yDeviceTypeGameController';
      case DeviceType.kKeyboard:
        return 'bluetoothA11yDeviceTypeKeyboard';
      case DeviceType.kKeyboardMouseCombo:
        return 'bluetoothA11yDeviceTypeKeyboardMouseCombo';
      case DeviceType.kMouse:
        return 'bluetoothA11yDeviceTypeMouse';
      case DeviceType.kTablet:
        return 'bluetoothA11yDeviceTypeTablet';
      default:
        assertNotReached();
    }
  }

  /**
   * @return {string}
   * @private
   */
  getSecondaryAriaLabel_() {
    const deviceName = this.getDeviceName_();
    switch (this.deviceItemState) {
      case DeviceItemState.FAILED:
        return this.i18n(
            'bluetoothPairingDeviceItemSecondaryErrorA11YLabel', deviceName);
      case DeviceItemState.PAIRING:
        return this.i18n(
            'bluetoothPairingDeviceItemSecondaryPairingA11YLabel', deviceName);
      case DeviceItemState.DEFAULT:
        return '';
      default:
        assertNotReached();
    }
  }
}

customElements.define(
    SettingsBluetoothPairingDeviceItemElement.is,
    SettingsBluetoothPairingDeviceItemElement);
