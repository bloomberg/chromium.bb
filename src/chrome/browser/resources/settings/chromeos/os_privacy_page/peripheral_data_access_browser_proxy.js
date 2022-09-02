// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.m.js';

/**
 * @fileoverview Helper browser proxy for peripheral data access client.
 */

/**
 * @typedef {{
 *     prefName: string,
 *     isUserConfigurable: boolean
 * }}
 */
export let DataAccessPolicyState;

/** @interface */
export class PeripheralDataAccessBrowserProxy {
  /**
   * @return {!Promise<boolean>}
   * Returns true if the device supports thunderbolt peripherals.
   */
  isThunderboltSupported() {}

  /**
   * @return {!Promise<DataAccessPolicyState>}
   * Returns the status of the policy,
   * kDeviceDevicePciPeripheralDataAccessEnabled.
   */
  getPolicyState() {}
}

/** @type {?PeripheralDataAccessBrowserProxy} */
let instance = null;

/** @implements {PeripheralDataAccessBrowserProxy} */
export class PeripheralDataAccessBrowserProxyImpl {
  /** @return {!PeripheralDataAccessBrowserProxy} */
  static getInstance() {
    return instance || (instance = new PeripheralDataAccessBrowserProxyImpl());
  }

  /** @param {!PeripheralDataAccessBrowserProxy} obj */
  static setInstance(obj) {
    instance = obj;
  }

  /**
   * @override
   * @return {!Promise<boolean>}
   */
  isThunderboltSupported() {
    return sendWithPromise('isThunderboltSupported');
  }

  /**
   * @override
   * @return {!Promise<DataAccessPolicyState>}
   */
  getPolicyState() {
    return sendWithPromise('getPolicyState');
  }
}
