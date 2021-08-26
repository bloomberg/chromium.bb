// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Settings subpage providing high level summary of the state of Bluetooth and
 * its connected devices.
 */

import '../../settings_shared_css.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.m.js';

import {I18nBehavior, I18nBehaviorInterface} from '//resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {getDeviceName} from 'chrome://resources/cr_components/chromeos/bluetooth/bluetooth_utils.js';
import {getBluetoothConfig} from 'chrome://resources/cr_components/chromeos/bluetooth/cros_bluetooth_config.js';

import {loadTimeData} from '../../i18n_setup.js';
import {Router} from '../../router.js';
import {routes} from '../os_route.m.js';

const mojom = chromeos.bluetoothConfig.mojom;

/**
 * Refers to Bluetooth secondary text label, used to distinguish between
 * accessibility string and UI text string.
 * @enum {number}
 */
const LabelType = {
  A11Y: 1,
  DISPLAYED_TEXT: 2
};

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const SettingsBluetoothSummaryElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
class SettingsBluetoothSummaryElement extends
    SettingsBluetoothSummaryElementBase {
  static get is() {
    return 'os-settings-bluetooth-summary';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * @type {!chromeos.bluetoothConfig.mojom.BluetoothSystemProperties}
       */
      systemProperties: {
        type: Object,
        observer: 'onSystemPropertiesChanged_',
      },

      /**
       * Reflects the current state of the toggle button. This will be set when
       * the |systemProperties| state changes or when the user presses the
       * toggle.
       * @private
       */
      isBluetoothToggleOn_: {
        type: Boolean,
        observer: 'onBluetoothToggleChanged_',
      },

      /** @private */
      LabelType: {
        type: Object,
        value: LabelType,
      }
    };
  }

  /** @private */
  onSystemPropertiesChanged_() {
    if (this.isToggleDisabled_()) {
      return;
    }
    this.isBluetoothToggleOn_ = this.systemProperties.systemState ===
            mojom.BluetoothSystemState.kEnabled ||
        this.systemProperties.systemState ===
            mojom.BluetoothSystemState.kEnabling;
  }

  /**
   * Observer for isBluetoothToggleOn_ that returns early until the previous
   * value was not undefined to avoid wrongly toggling the Bluetooth state.
   * @param {boolean} newValue
   * @param {boolean} oldValue
   * @private
   */
  onBluetoothToggleChanged_(newValue, oldValue) {
    if (oldValue === undefined) {
      return;
    }
    getBluetoothConfig().setBluetoothEnabledState(this.isBluetoothToggleOn_);
  }

  /**
   * @return {boolean}
   * @private
   */
  isToggleDisabled_() {
    if (!this.systemProperties) {
      return false;
    }
    // TODO(crbug.com/1010321): Add check for modification state when variable
    // is available.
    return this.systemProperties.systemState ===
        mojom.BluetoothSystemState.kUnavailable;
  }

  /**
   * @param {LabelType} labelType
   * @return {string}
   * @private
   */
  getSecondaryLabel_(labelType) {
    if (!this.isBluetoothToggleOn_) {
      return this.i18n('bluetoothSummaryPageOff');
    }

    const connectedDevices = this.getConnectedDevices_();

    if (!connectedDevices.length) {
      return this.i18n('bluetoothSummaryPageOn');
    }

    const isA11yLabel = labelType === LabelType.A11Y;
    const firstConnectedDeviceName = getDeviceName(connectedDevices[0]);

    if (connectedDevices.length === 1) {
      return isA11yLabel ? this.i18n(
                               'bluetoothSummaryPageConnectedA11yOneDevice',
                               firstConnectedDeviceName) :
                           firstConnectedDeviceName;
    }

    if (connectedDevices.length === 2) {
      const secondConnectedDeviceName = getDeviceName(connectedDevices[1]);
      return isA11yLabel ?
          this.i18n(
              'bluetoothSummaryPageConnectedA11yTwoDevices',
              firstConnectedDeviceName, secondConnectedDeviceName) :
          this.i18n(
              'bluetoothSummaryPageTwoDevicesDescription',
              firstConnectedDeviceName, secondConnectedDeviceName);
    }

    return isA11yLabel ?
        this.i18n(
            'bluetoothSummaryPageConnectedA11yTwoOrMoreDevices',
            firstConnectedDeviceName, connectedDevices.length - 1) :
        this.i18n(
            'bluetoothSummaryPageTwoOrMoreDevicesDescription',
            firstConnectedDeviceName, connectedDevices.length - 1);
  }

  /**
   * @return {Array<?chromeos.bluetoothConfig.mojom.PairedBluetoothDeviceProperties>}
   * @private
   */
  getConnectedDevices_() {
    const pairedDevices = this.systemProperties.pairedDevices;
    if (!pairedDevices) {
      return [];
    }

    return pairedDevices.filter(
        device => device.deviceProperties.connectionState ===
            mojom.DeviceConnectionState.kConnected);
  }

  /**
   * @return {string}
   * @private
   */
  getBluetoothStatusIconName_() {
    if (!this.isBluetoothToggleOn_) {
      return 'os-settings:bluetooth-disabled';
    }

    if (this.getConnectedDevices_().length) {
      return 'os-settings:bluetooth-connected';
    }
    return 'cr:bluetooth';
  }

  /**
   * @return {boolean}
   * @private
   */
  shouldShowSubpageArrow_() {
    if (this.isToggleDisabled_()) {
      return false;
    }

    return this.isBluetoothToggleOn_;
  }

  /**
   * @param {!Event} e
   * @private
   */
  onSubpageArrowClick_(e) {
    this.navigateToBluetoothDevicesSubpage_();
    e.stopPropagation();
  }

  /** @private */
  navigateToBluetoothDevicesSubpage_() {
    Router.getInstance().navigateTo(routes.BLUETOOTH_DEVICES);
  }

  /** @private */
  onWrapperClick_() {
    if (this.isToggleDisabled_()) {
      return;
    }

    if (this.systemProperties.systemState ===
            mojom.BluetoothSystemState.kDisabled ||
        this.systemProperties.systemState ===
            mojom.BluetoothSystemState.kDisabling) {
      this.isBluetoothToggleOn_ = true;
      return;
    }
    this.navigateToBluetoothDevicesSubpage_();
  }
}

customElements.define(
    SettingsBluetoothSummaryElement.is, SettingsBluetoothSummaryElement);
