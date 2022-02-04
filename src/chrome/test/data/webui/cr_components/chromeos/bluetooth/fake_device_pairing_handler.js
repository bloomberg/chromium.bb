// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// cros_bluetooth_config.mojom-lite.js depends on url.mojom.Url.
import 'chrome://resources/mojo/url/mojom/url.mojom-lite.js';
// TODO(crbug.com/1010321): Use cros_bluetooth_config.mojom-webui.js instead
// as non-module JS is deprecated.
import 'chrome://resources/mojo/chromeos/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-lite.js';
import {PairingAuthType} from 'chrome://resources/cr_components/chromeos/bluetooth/bluetooth_types.js';
import {assert} from 'chrome://resources/js/assert.m.js';

/**
 * @fileoverview Fake implementation of DevicePairingHandler for testing.
 */

/**
 * @implements {chromeos.bluetoothConfig.mojom.DevicePairingHandlerInterface}
 */
export class FakeDevicePairingHandler {
  constructor() {
    /**
     * @private {?chromeos.bluetoothConfig.mojom.DevicePairingDelegateInterface}
     */
    this.devicePairingDelegate_ = null;

    /**
     * @private {?function({result:
     *     chromeos.bluetoothConfig.mojom.PairingResult})}
     */
    this.pairDeviceCallback_ = null;

    /**
     * @private {?function()}
     */
    this.pairDeviceRejectCallback_ = null;

    /** @private {number} */
    this.pairDeviceCalledCount_ = 0;

    /** @private {string} */
    this.pinOrPasskey_ = '';

    /** @private {boolean} */
    this.confirmPasskeyResult_ = false;

    /** @private {?chromeos.bluetoothConfig.mojom.KeyEnteredHandlerRemote} */
    this.lastKeyEnteredHandlerRemote_ = null;

    /** @private {?function()} */
    this.waitForPairDeviceCallback_ = null;

    /** @private {?function()} */
    this.finishRequestConfirmPasskeyCallback_ = null;
  }

  /** @override */
  pairDevice(deviceId, delegate) {
    this.pairDeviceCalledCount_++;
    this.devicePairingDelegate_ = delegate;
    let promise = new Promise((resolve, reject) => {
      this.pairDeviceCallback_ = resolve;
      this.pairDeviceRejectCallback_ = reject;
    });

    if (this.waitForPairDeviceCallback_) {
      this.waitForPairDeviceCallback_();
    }

    return promise;
  }

  /**
   * Returns a promise that will be resolved the next time
   * pairDevice() is called.
   * @return {Promise}
   */
  waitForPairDevice() {
    return new Promise((resolve) => {
      this.waitForPairDeviceCallback_ = resolve;
    });
  }

  /**
   * Second step in pair device operation. This method should be called
   * after pairDevice(). Pass in a |PairingAuthType| to simulate each
   * pairing request made to |DevicePairingDelegate|.
   * @param {!PairingAuthType} authType
   * @param {string=} opt_pairingCode used in confirm passkey and display
   * passkey/PIN authentication.
   */
  requireAuthentication(authType, opt_pairingCode) {
    assert(this.devicePairingDelegate_, 'devicePairingDelegate_ was not set.');
    switch (authType) {
      case PairingAuthType.REQUEST_PIN_CODE:
        this.devicePairingDelegate_.requestPinCode()
            .then(
                (response) => this.finishRequestPinOrPasskey_(response.pinCode))
            .catch(e => {});
        break;
      case PairingAuthType.REQUEST_PASSKEY:
        this.devicePairingDelegate_.requestPasskey()
            .then(
                (response) => this.finishRequestPinOrPasskey_(response.passkey))
            .catch(e => {});
        break;
      case PairingAuthType.DISPLAY_PIN_CODE:
        assert(opt_pairingCode);
        this.devicePairingDelegate_.displayPinCode(
            opt_pairingCode, this.getKeyEnteredHandlerPendingReceiver_());
        break;
      case PairingAuthType.DISPLAY_PASSKEY:
        assert(opt_pairingCode);
        this.devicePairingDelegate_.displayPasskey(
            opt_pairingCode, this.getKeyEnteredHandlerPendingReceiver_());
        break;
      case PairingAuthType.CONFIRM_PASSKEY:
        assert(opt_pairingCode);
        this.devicePairingDelegate_.confirmPasskey(opt_pairingCode)
            .then(
                (response) =>
                    this.finishRequestConfirmPasskey_(response.confirmed))
            .catch(e => {});
        break;
      case PairingAuthType.AUTHORIZE_PAIRING:
        // TODO(crbug.com/1010321): Implement this.
        break;
    }
  }

  /**
   * Returns a promise that will be resolved the next time
   * finishRequestConfirmPasskey_() is called.
   * @return {Promise}
   */
  waitForFinishRequestConfirmPasskey_() {
    return new Promise((resolve) => {
      this.finishRequestConfirmPasskeyCallback_ = resolve;
    });
  }

  /**
   * @return {!chromeos.bluetoothConfig.mojom.KeyEnteredHandlerPendingReceiver}
   * @private
   */
  getKeyEnteredHandlerPendingReceiver_() {
    this.lastKeyEnteredHandlerRemote_ =
        new chromeos.bluetoothConfig.mojom.KeyEnteredHandlerRemote();
    return this.lastKeyEnteredHandlerRemote_.$.bindNewPipeAndPassReceiver();
  }

  /**s
   * @return {?chromeos.bluetoothConfig.mojom.KeyEnteredHandlerRemote}
   */
  getLastKeyEnteredHandlerRemote() {
    return this.lastKeyEnteredHandlerRemote_;
  }

  /**
   * @param {string} code
   * @private
   */
  finishRequestPinOrPasskey_(code) {
    this.pinOrPasskey_ = code;
  }

  /**
   * @param {boolean} confirmed
   * @private
   */
  finishRequestConfirmPasskey_(confirmed) {
    this.confirmPasskeyResult_ = confirmed;

    if (this.finishRequestConfirmPasskeyCallback_) {
      this.finishRequestConfirmPasskeyCallback_();
    }
  }

  /**
   * Ends the operation to pair a bluetooth device. This method should be called
   * after pairDevice() has been called. It can be called without calling
   * pairingRequest() method to simulate devices being paired with no pin or
   * passkey required.
   * @param {boolean} success
   */
  completePairDevice(success) {
    assert(this.pairDeviceCallback_, 'pairDevice() was never called.');
    this.pairDeviceCallback_({
      result: success ? chromeos.bluetoothConfig.mojom.PairingResult.kSuccess :
                        chromeos.bluetoothConfig.mojom.PairingResult.kAuthFailed
    });
  }

  /**
   * Simulates pairing failing due to an exception, such as the Mojo pipe
   * disconnecting.
   */
  rejectPairDevice() {
    if (this.pairDeviceRejectCallback_) {
      this.pairDeviceRejectCallback_();
    }
  }

  /**
   * @return {number}
   */
  getPairDeviceCalledCount() {
    return this.pairDeviceCalledCount_;
  }

  /** @return {string} */
  getPinOrPasskey() {
    return this.pinOrPasskey_;
  }

  /** @return {boolean} */
  getConfirmPasskeyResult() {
    return this.confirmPasskeyResult_;
  }
}
