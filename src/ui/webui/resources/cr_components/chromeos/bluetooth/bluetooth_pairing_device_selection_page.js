// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * UI element to show a list of discovered Bluetooth devices and initiate
 * pairing to a device.
 */
import './bluetooth_base_page.js';
import './bluetooth_pairing_device_item.js';
import '../../../cr_elements/shared_style_css.m.js';
import '//resources/polymer/v3_0/iron-list/iron-list.js';
import '../localized_link/localized_link.js';

import {CrScrollableBehavior, CrScrollableBehaviorInterface} from '//resources/cr_elements/cr_scrollable_behavior.m.js';
import {I18nBehavior, I18nBehaviorInterface} from '//resources/js/i18n_behavior.m.js';
import {afterNextRender, html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ButtonBarState, ButtonState, DeviceItemState} from './bluetooth_types.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {CrScrollableBehaviorInterface}
 * @implements {I18nBehaviorInterface}
 */
const SettingsBluetoothPairingDeviceSelectionPageElementBase =
    mixinBehaviors([CrScrollableBehavior, I18nBehavior], PolymerElement);

/** @polymer */
export class SettingsBluetoothPairingDeviceSelectionPageElement extends
    SettingsBluetoothPairingDeviceSelectionPageElementBase {
  static get is() {
    return 'bluetooth-pairing-device-selection-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * @type {!Array<!chromeos.bluetoothConfig.mojom.BluetoothDeviceProperties>}
       */
      devices: {
        type: Array,
        value: [],
        observer: 'onDevicesChanged_',
      },

      /**
       * Id of a device who's pairing attempt failed.
       * @type {string}
       */
      failedPairingDeviceId: {
        type: String,
        value: '',
      },

      /**
       * @type {?chromeos.bluetoothConfig.mojom.BluetoothDeviceProperties}
       */
      devicePendingPairing: {
        type: Object,
        value: null,
        observer: 'onDevicePendingPairingChanged_',
      },

      /** @type {boolean} */
      isBluetoothEnabled: {
        type: Boolean,
        value: false,
      },

      /** @private {!ButtonBarState} */
      buttonBarState_: {
        type: Object,
        value: {
          cancel: ButtonState.ENABLED,
          pair: ButtonState.HIDDEN,
        },
      },

      /**
       * Used by FocusRowBehavior to track the last focused element on a row.
       * @private
       */
      lastFocused_: Object,

      /**
       * Used by FocusRowBehavior to track if the list has been blurred.
       * @private
       */
      listBlurred_: Boolean,
    };
  }

  constructor() {
    super();

    /**
     * The last device that was selected for pairing.
     * @private {?chromeos.bluetoothConfig.mojom.BluetoothDeviceProperties}
     */
    this.lastSelectedDevice_ = null;
  }

  /**
   * Attempts to focus the item corresponding to |lastSelectedDevice_|.
   */
  attemptFocusLastSelectedItem() {
    if (!this.lastSelectedDevice_) {
      return;
    }

    const index = this.devices.findIndex(
        device => device.id === this.lastSelectedDevice_.id);
    if (index < 0) {
      return;
    }

    afterNextRender(this, function() {
      const items =
          this.shadowRoot.querySelectorAll('bluetooth-pairing-device-item');
      if (index >= items.length) {
        return;
      }

      items[index].focus();
    });
  }

  /** @private */
  onDevicesChanged_() {
    // CrScrollableBehaviorInterface method required for list items to be
    // properly rendered when devices updates. This is because iron-list size
    // is not fixed, if this is not called iron-list container would not be
    // properly sized.
    this.updateScrollableContents();
  }

  /** @private */
  onDevicePendingPairingChanged_() {
    // If |devicePendingPairing_| has changed to a defined value, it was the
    // last selected device. |devicePendingPairing_| gets reset to null whenever
    // we move back to this page after a pairing attempt fails or cancels. In
    // this case, do not reset |lastSelectedDevice_| because we want to hold
    // onto the device that was last attempted to be paired with.
    if (!this.devicePendingPairing) {
      return;
    }

    this.lastSelectedDevice_ = this.devicePendingPairing;
  }

  /**
   * @private
   * @return {boolean}
   */
  shouldShowDeviceList_() {
    return this.isBluetoothEnabled && this.devices && this.devices.length > 0;
  }

  /**
   * @private
   * @return {string}
   */
  getDeviceListTitle_() {
    if (!this.isBluetoothEnabled) {
      return this.i18n('bluetoothDisabled');
    }

    if (this.shouldShowDeviceList_()) {
      return this.i18n('bluetoothAvailableDevices');
    }

    return this.i18n('bluetoothNoAvailableDevices');
  }

  /**
   * @param {?chromeos.bluetoothConfig.mojom.BluetoothDeviceProperties} device
   * @return {!DeviceItemState}
   * @private
   */
  getDeviceItemState_(device) {
    if (!device) {
      return DeviceItemState.DEFAULT;
    }

    if (device.id === this.failedPairingDeviceId) {
      return DeviceItemState.FAILED;
    }

    if (this.devicePendingPairing &&
        device.id === this.devicePendingPairing.id) {
      return DeviceItemState.PAIRING;
    }

    return DeviceItemState.DEFAULT;
  }
}

customElements.define(
    SettingsBluetoothPairingDeviceSelectionPageElement.is,
    SettingsBluetoothPairingDeviceSelectionPageElement);
