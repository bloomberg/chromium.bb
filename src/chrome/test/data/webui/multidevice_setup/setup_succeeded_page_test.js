// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Suite of tests for page-specific behaviors of
 * SetupSucceededPage.
 */

/**
 * @implements {multidevice_setup.BrowserProxy}
 */
class TestMultideviceSetupBrowserProxy extends TestBrowserProxy {
  constructor() {
    super(['getProfileInfo', 'openMultiDeviceSettings']);
  }

  /** @override */
  openMultiDeviceSettings() {
    this.methodCalled('openMultiDeviceSettings');
  }

  /** @override */
  getProfileInfo() {
    this.methodCalled('getProfileInfo');
    return Promise.resolve({});
  }
}

cr.define('multidevice_setup', () => {
  function registerSetupSucceededPageTests() {
    suite('MultiDeviceSetup', () => {
      /**
       * SetupSucceededPage created before each test. Defined in setUp.
       * @type {?SetupSucceededPage}
       */
      let setupSucceededPageElement = null;
      /** @type {?TestMultideviceSetupBrowserProxy} */
      let browserProxy = null;

      setup(() => {
        browserProxy = new TestMultideviceSetupBrowserProxy();
        multidevice_setup.BrowserProxyImpl.instance_ = browserProxy;

        setupSucceededPageElement =
            document.createElement('setup-succeeded-page');
        document.body.appendChild(setupSucceededPageElement);
      });

      test('Settings link opens settings page', () => {
        setupSucceededPageElement.$$('#settings-link').click();
        return browserProxy.whenCalled('openMultiDeviceSettings');
      });
    });
  }
  return {registerSetupSucceededPageTests: registerSetupSucceededPageTests};
});
