// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shimless_rma_shared_css.js';
import './base_page.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {HardwareWriteProtectionStateObserverInterface, HardwareWriteProtectionStateObserverReceiver, ShimlessRmaServiceInterface, StateResult} from './shimless_rma_types.js';

// TODO(gavindodd): Update text for i18n
const openDeviceMessage = 'Open your device and reconnect the battery.';
const hwwpEnabledMessage = 'HWWP enabled.';

/**
 * @fileoverview
 * 'wrapup-wait-for-manual-wp-enable-page' wait for the manual HWWP enable to be
 * completed.
 */
export class WrapupWaitForManualWpEnablePageElement extends PolymerElement {
  static get is() {
    return 'wrapup-wait-for-manual-wp-enable-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @protected */
      hwwpEnabled_: {
        type: Boolean,
        value: false,
      },
    };
  }

  constructor() {
    super();
    /** @private {ShimlessRmaServiceInterface} */
    this.shimlessRmaService_ = getShimlessRmaService();
    /**
     * Receiver responsible for observing hardware write protection state.
     * @private {
     *  ?HardwareWriteProtectionStateObserverReceiver}
     */
    this.hardwareWriteProtectionStateObserverReceiver_ =
        new HardwareWriteProtectionStateObserverReceiver(
            /**
             * @type {!HardwareWriteProtectionStateObserverInterface}
             */
            (this));

    this.shimlessRmaService_.observeHardwareWriteProtectionState(
        this.hardwareWriteProtectionStateObserverReceiver_.$
            .bindNewPipeAndPassRemote());
  }

  /**
   * @protected
   * @param {boolean} hwwpEnabled
   * @return {string}
   */
  getBodyText_(hwwpEnabled) {
    return !this.hwwpEnabled_ ? openDeviceMessage : hwwpEnabledMessage;
  }

  /**
   * @param {boolean} enabled
   */
  onHardwareWriteProtectionStateChanged(enabled) {
    this.hwwpEnabled_ = enabled;
    // TODO(gavindodd): enable/disable next button. Or should it automatically
    // progress to the next state?
    this.dispatchEvent(new CustomEvent(
        'disable-next-button',
        {bubbles: true, composed: true, detail: !this.hwwpEnabled_},
        ));
  }

  /** @return {!Promise<!StateResult>} */
  onNextButtonClick() {
    if (this.hwwpEnabled_) {
      return this.shimlessRmaService_.writeProtectManuallyEnabled();
    } else {
      return Promise.reject(
          new Error('Hardware Write Protection is not enabled.'));
    }
  }
}

customElements.define(
    WrapupWaitForManualWpEnablePageElement.is,
    WrapupWaitForManualWpEnablePageElement);
