// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestBrowserProxy} from '../../test_browser_proxy.m.js';

/**
 * @implements {BluetoothPageBrowserProxy}
 */
export class TestBluetoothPageBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'isDeviceBlockedByPolicy',
    ]);
    /** @private {boolean} */
    this.isDeviceBlockedByPolicy_ = false;
  }

  /** @override */
  isDeviceBlockedByPolicy(address) {
    this.methodCalled('isDeviceBlockedByPolicy');
    return Promise.resolve(this.isDeviceBlockedByPolicy_);
  }

  /**
   * @param {boolean} isDeviceBlockedByPolicy
   */
  setIsDeviceBlockedByPolicyForTest(isDeviceBlockedByPolicy) {
    this.isDeviceBlockedByPolicy_ = isDeviceBlockedByPolicy;
  }
}
