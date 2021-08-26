// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.js';
// clang-format on

/**
 * Test value for messages for web permissions origin.
 */
export const TEST_ANDROID_SMS_ORIGIN = 'http://foo.com';

/** @implements {AndroidInfoBrowserProxy} */
export class TestAndroidInfoBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'getAndroidSmsInfo',
    ]);
    this.androidSmsInfo = {origin: TEST_ANDROID_SMS_ORIGIN, enabled: true};
  }

  /** @override */
  getAndroidSmsInfo() {
    this.methodCalled('getAndroidSmsInfo');
    return Promise.resolve(this.androidSmsInfo);
  }
}
