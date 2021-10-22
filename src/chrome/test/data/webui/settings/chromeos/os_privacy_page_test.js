// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/lazy_load.js';

// #import {TestBrowserProxy} from '../../test_browser_proxy.js';
// #import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
// #import {flush} from'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
// #import {assert} from 'chrome://resources/js/assert.m.js';
// #import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
// #import {SecureDnsMode, SecureDnsUiManagementMode, Router, routes, PeripheralDataAccessBrowserProxyImpl, DataAccessPolicyState} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {FakeQuickUnlockPrivate} from './fake_quick_unlock_private.m.js';
// #import {waitAfterNextRender} from 'chrome://test/test_util.js';
// clang-format on

const crosSettingPrefName = 'cros.device.peripheral_data_access_enabled';
const localStatePrefName =
    'settings.local_state_device_pci_data_access_enabled';

/**
 * @implements {settings.PeripheralDataAccessBrowserProxy}
 */
class TestPeripheralDataAccessBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'isThunderboltSupported',
      'getPolicyState',
    ]);

    /** @type {DataAccessPolicyState} */
    this.policy_state_ = {
      prefName: crosSettingPrefName,
      isUserConfigurable: false
    };
  }

  /** @override */
  isThunderboltSupported() {
    this.methodCalled('isThunderboltSupported');
    return Promise.resolve(/*supported=*/ true);
  }

  /** @override */
  getPolicyState() {
    this.methodCalled('getPolicyState');
    return Promise.resolve(this.policy_state_);
  }

  /**
   * @param {String} pref_name
   * @param {Boolean} is_user_configurable
   */
  setPolicyState(pref_name, is_user_configurable) {
    this.policy_state_.prefName = pref_name;
    this.policy_state_.isUserConfigurable = is_user_configurable;
  }
}

suite('PrivacyPageTests', function() {
  /** @type {SettingsPrivacyPageElement} */
  let privacyPage = null;

  const prefs_ = {
    'cros': {
      'device': {
        'peripheral_data_access_enabled': {
          value: true,
        }
      }
    },
  };

  /** @type {?TestPeripheralDataAccessBrowserProxy} */
  let browserProxy = null;

  setup(async () => {
    browserProxy = new TestPeripheralDataAccessBrowserProxy();
    settings.PeripheralDataAccessBrowserProxyImpl.instance_ = browserProxy;
    loadTimeData.overrideValues({
      pciguardUiEnabled: false,
    });

    PolymerTest.clearBody();
    privacyPage = document.createElement('os-settings-privacy-page');
    document.body.appendChild(privacyPage);
    Polymer.dom.flush();

    await browserProxy.whenCalled('isThunderboltSupported');
  });

  teardown(function() {
    privacyPage.remove();
    settings.Router.getInstance().resetRouteForTesting();
  });

  test('Suggested content, pref disabled', async () => {
    privacyPage = document.createElement('os-settings-privacy-page');
    document.body.appendChild(privacyPage);
    Polymer.dom.flush();

    // The default state of the pref is disabled.
    const suggestedContent = assert(privacyPage.$$('#suggested-content'));
    assertFalse(suggestedContent.checked);
  });

  test('Suggested content, pref enabled', async () => {
    // Update the backing pref to enabled.
    privacyPage.prefs = {
      'settings': {
        'suggested_content_enabled': {
          value: true,
        }
      },
      'cros': {
        'device': {
          'peripheral_data_access_enabled': {
            value: true,
          }
        }
      },
      'dns_over_https': {
        'mode': {
          value: SecureDnsMode.AUTOMATIC
        },
        'templates': {
          value: ''
        }
      }
    };

    Polymer.dom.flush();

    // The checkbox reflects the updated pref state.
    const suggestedContent = assert(privacyPage.$$('#suggested-content'));
    assertTrue(suggestedContent.checked);
  });

  test('Deep link to verified access', async () => {
    const params = new URLSearchParams;
    params.append('settingId', '1101');
    settings.Router.getInstance().navigateTo(
        settings.routes.OS_PRIVACY, params);

    Polymer.dom.flush();

    const deepLinkElement = privacyPage.$$('#enableVerifiedAccess')
                                .shadowRoot.querySelector('cr-toggle');
    await test_util.waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Verified access toggle should be focused for settingId=1101.');
  });

  test('Fingerprint dialog closes when token expires', async () => {
    loadTimeData.overrideValues({
      fingerprintUnlockEnabled: true,
    });

    privacyPage = document.createElement('os-settings-privacy-page');
    document.body.appendChild(privacyPage);

    await test_util.waitAfterNextRender(privacyPage);

    if (!privacyPage.isAccountManagementFlowsV2Enabled_) {
      return;
    }

    const quickUnlockPrivateApi = new settings.FakeQuickUnlockPrivate();
    privacyPage.authToken_ = quickUnlockPrivateApi.getFakeToken();

    settings.Router.getInstance().navigateTo(settings.routes.LOCK_SCREEN);
    Polymer.dom.flush();

    const subpageTrigger = privacyPage.$$('#lockScreenSubpageTrigger');
    // Sub-page trigger navigates to the lock screen page.
    subpageTrigger.click();
    Polymer.dom.flush();

    assertEquals(
        settings.Router.getInstance().getCurrentRoute(),
        settings.routes.LOCK_SCREEN);
    const lockScreenPage = assert(privacyPage.$$('#lockScreen'));

    // Password dialog should not open because the authToken_ is set.
    assertFalse(privacyPage.showPasswordPromptDialog_);

    const editFingerprintsTrigger = lockScreenPage.$$('#editFingerprints');
    editFingerprintsTrigger.click();
    Polymer.dom.flush();

    assertEquals(
        settings.Router.getInstance().getCurrentRoute(),
        settings.routes.FINGERPRINT);
    assertFalse(privacyPage.showPasswordPromptDialog_);

    const fingerprintTrigger =
        privacyPage.$$('#fingerprint-list').$$('#addFingerprint');
    fingerprintTrigger.click();

    // Invalidate the auth token by firing an event.
    assertFalse(privacyPage.authToken_ === undefined);
    const event = new CustomEvent('invalidate-auth-token-requested');
    lockScreenPage.dispatchEvent(event);
    assertTrue(privacyPage.authToken_ === undefined);

    assertEquals(
        settings.Router.getInstance().getCurrentRoute(),
        settings.routes.FINGERPRINT);
    assertTrue(privacyPage.showPasswordPromptDialog_);
  });
});

suite('PrivacePageTest_OfficialBuild', async () => {
  /** @type {SettingsPrivacyPageElement} */
  let privacyPage = null;

  const prefs_ = {
    'cros': {
      'device': {
        'peripheral_data_access_enabled': {
          value: true,
        }
      }
    },
   };

  /** @type {?TestPeripheralDataAccessBrowserProxy} */
  let browserProxy = null;

  setup(async () => {
    browserProxy = new TestPeripheralDataAccessBrowserProxy();
    settings.PeripheralDataAccessBrowserProxyImpl.instance_ = browserProxy;
    loadTimeData.overrideValues({
      pciguardUiEnabled: false,
    });

    PolymerTest.clearBody();
    privacyPage = document.createElement('os-settings-privacy-page');
    document.body.appendChild(privacyPage);
    Polymer.dom.flush();

    await browserProxy.whenCalled('isThunderboltSupported');
  });

  teardown(function() {
    privacyPage.remove();
    settings.Router.getInstance().resetRouteForTesting();
  });

  test('Deep link to send usage stats', async () => {
    const params = new URLSearchParams;
    params.append('settingId', '1103');
    settings.Router.getInstance().navigateTo(
        settings.routes.OS_PRIVACY, params);

    Polymer.dom.flush();

    const deepLinkElement =
        privacyPage.$$('#enable-logging').shadowRoot.querySelector('cr-toggle');
    await test_util.waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Send usage stats toggle should be focused for settingId=1103.');
  });
});

suite('PeripheralDataAccessTest', function() {
  /** @type {SettingsPrivacyPageElement} */
  let privacyPage = null;

  /** @type {Object} */
  const prefs_ = {
    'cros': {
      'device': {
        'peripheral_data_access_enabled': {
          value: false,
        }
      }
    },
    'settings': {'local_state_device_pci_data_access_enabled': {value: false}},
    'dns_over_https': {
      'mode': {
        value: SecureDnsMode.AUTOMATIC
      },
      'templates': {
        value: ''
      }
     },
   };

  /** @type {?TestPeripheralDataAccessBrowserProxy} */
  let browserProxy = null;

  setup(async () => {
    browserProxy = new TestPeripheralDataAccessBrowserProxy();
    settings.PeripheralDataAccessBrowserProxyImpl.instance_ = browserProxy;
    PolymerTest.clearBody();
    loadTimeData.overrideValues({
      pciguardUiEnabled: true,
    });
  });

  teardown(function() {
    privacyPage.remove();
    settings.Router.getInstance().resetRouteForTesting();
  });

  async function setUpPage(pref_name, is_user_configurable) {
    browserProxy.setPolicyState(pref_name, is_user_configurable);
    privacyPage = document.createElement('os-settings-privacy-page');
    privacyPage.prefs = Object.assign({}, prefs_);
    document.body.appendChild(privacyPage);
    Polymer.dom.flush();

    await browserProxy.whenCalled('getPolicyState');
    await test_util.waitAfterNextRender();
    Polymer.dom.flush();
  }

  test('DialogOpensOnToggle', async () => {
    await setUpPage(crosSettingPrefName, /**is_user_configurable=*/ true);
    // The default state is checked.
    const toggle = privacyPage.$$('#crosSettingDataAccessToggle');
    assertTrue(!!toggle);
    assertTrue(toggle.checked);

    // Attempting to switch the toggle off will result in the warning dialog
    // appearing.
    toggle.click();
    Polymer.dom.flush();

    await test_util.waitAfterNextRender(privacyPage);

    const dialog = privacyPage.$$('#protectionDialog').$.warningDialog;
    assertTrue(dialog.open);

    // Ensure that the toggle is still checked.
    assertTrue(toggle.checked);

    // Click on the dialog's cancel button and expect the toggle to switch back
    // to enabled.
    const cancelButton = dialog.querySelector('#cancelButton');
    cancelButton.click();
    Polymer.dom.flush();
    assertFalse(dialog.open);

    // The toggle should not have changed position.
    assertTrue(toggle.checked);
  });

  test('DisableClicked', async () => {
    await setUpPage(crosSettingPrefName, /**is_user_configurable=*/ true);
    // The default state is checked.
    const toggle = privacyPage.$$('#crosSettingDataAccessToggle');
    assertTrue(!!toggle);
    assertTrue(toggle.checked);

    // Attempting to switch the toggle off will result in the warning dialog
    // appearing.
    toggle.click();
    Polymer.dom.flush();

    await test_util.waitAfterNextRender(privacyPage);

    const dialog = privacyPage.$$('#protectionDialog').$.warningDialog;
    assertTrue(dialog.open);

    // Advance the dialog and move onto the next dialog.
    const disableButton = dialog.querySelector('#disableConfirmation');
    disableButton.click();
    Polymer.dom.flush();

    // The toggle should now be flipped to unset.
    assertFalse(toggle.checked);
  });

  test('managedAndConfigurablePrefIsToggleable', async () => {
    await setUpPage(localStatePrefName, /**is_user_configurable=*/ true);
    Polymer.dom.flush();

    // Ensure only the local state toggle appears.
    assertTrue(privacyPage.$$('#crosSettingDataAccessToggle').hidden);

    // The default state is checked.
    const toggle = privacyPage.$$('#localStateDataAccessToggle');

    // The default state is checked.
    assertTrue(!!toggle);
    assertTrue(toggle.checked);

    // Attempting to switch the toggle off will result in the warning dialog
    // appearing.
    toggle.click();
    Polymer.dom.flush();

    await test_util.waitAfterNextRender(privacyPage);

    const dialog = privacyPage.$$('#protectionDialog').$.warningDialog;
    assertTrue(dialog.open);

    // Ensure that the toggle is still checked.
    assertTrue(toggle.checked);

    // Click on the dialog's cancel button and expect the toggle to switch back
    // to enabled.
    const cancelButton = dialog.querySelector('#cancelButton');
    cancelButton.click();
    Polymer.dom.flush();
    assertFalse(dialog.open);

    // The toggle should not have changed position.
    assertTrue(toggle.checked);
  });

  test('managedAndNonConfigurablePrefIsNotToggleable', async () => {
    await setUpPage(localStatePrefName, /**is_user_configurable=*/ false);
    Polymer.dom.flush();

    // Ensure only the local state toggle appears.
    assertTrue(privacyPage.$$('#crosSettingDataAccessToggle').hidden);

    // The default state is checked.
    const toggle = privacyPage.$$('#localStateDataAccessToggle');

    // The default state is checked.
    assertTrue(!!toggle);
    assertTrue(toggle.checked);

    // Attempting to switch the toggle off will result in the warning dialog
    // appearing.
    toggle.click();
    Polymer.dom.flush();

    await test_util.waitAfterNextRender(privacyPage);

    // Dialog should not appear since the toggle is disabled.
    const dialog = privacyPage.$$('#protectionDialog');
    assertFalse(!!dialog);
  });
});
