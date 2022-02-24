// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {TestBrowserProxy} from '../../test_browser_proxy.js';
// #import {assertEquals} from '../../chai_assert.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {NearbyAccountManagerBrowserProxy, NearbyAccountManagerBrowserProxyImpl, setNearbyShareSettingsForTesting, setReceiveManagerForTesting, setContactManagerForTesting, Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {FakeContactManager} from '../../nearby_share/shared/fake_nearby_contact_manager.m.js';
// #import {FakeNearbyShareSettings} from '../../nearby_share/shared/fake_nearby_share_settings.m.js';
// #import {FakeReceiveManager} from './fake_receive_manager.m.js'
// #import {isVisible, waitAfterNextRender} from 'chrome://test/test_util.js';
// #import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
// clang-format on

/** @implements {nearby_share.AccountManagerBrowserProxy} */
class TestAccountManagerBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'getAccounts',
    ]);
  }

  /** @override */
  getAccounts() {
    this.methodCalled('getAccounts');

    return Promise.resolve([
      {
        id: '123',
        accountType: 1,
        isDeviceAccount: true,
        isSignedIn: true,
        unmigrated: false,
        fullName: 'Primary Account',
        pic: 'data:image/png;base64,primaryAccountPicData',
        email: 'primary@gmail.com',
      },
    ]);
  }
}

suite('NearbyShare', function() {
  /** @type {?SettingsNearbyShareSubpage} */
  let subpage = null;
  /** @type {?SettingsToggleButtonElement} */
  let featureToggleButton = null;
  /** @type {?FakeReceiveManager} */
  let fakeReceiveManager = null;
  /** @type {nearby_share.AccountManagerBrowserProxy} */
  let accountManagerBrowserProxy = null;
  /** @type {!nearby_share.FakeContactManager} */
  let fakeContactManager = null;
  /** @type {!nearby_share.FakeNearbyShareSettings} */
  let fakeSettings = null;

  setup(function() {
    setupFakes();
    fakeSettings.setEnabled(true);
    fakeSettings.setIsOnboardingComplete(true);


    createSubpage(/*is_enabled=*/ true, /*is_onboarding_complete=*/ true);
    syncFakeSettings();
    featureToggleButton = subpage.$$('#featureToggleButton');

    return flushAsync();
  });

  teardown(function() {
    subpage.remove();
    settings.Router.getInstance().resetRouteForTesting();
  });

  function setupFakes() {
    accountManagerBrowserProxy = new TestAccountManagerBrowserProxy();
    nearby_share.NearbyAccountManagerBrowserProxyImpl.instance_ =
        accountManagerBrowserProxy;

    fakeReceiveManager = new nearby_share.FakeReceiveManager();
    nearby_share.setReceiveManagerForTesting(fakeReceiveManager);

    fakeContactManager = new nearby_share.FakeContactManager();
    nearby_share.setContactManagerForTesting(fakeContactManager);
    fakeContactManager.setupContactRecords();

    fakeSettings = new nearby_share.FakeNearbyShareSettings();
    nearby_share.setNearbyShareSettingsForTesting(fakeSettings);
  }

  function syncFakeSettings() {
    subpage.set('settings.enabled', fakeSettings.getEnabledForTest());
    subpage.set(
        'settings.isFastInitiationHardwareSupported',
        fakeSettings.getIsFastInitiationHardwareSupportedTest());
    subpage.set(
        'settings.fastInitiationNotificationState',
        fakeSettings.getFastInitiationNotificationStateTest());
    subpage.set('settings.deviceName', fakeSettings.getDeviceNameForTest());
    subpage.set('settings.dataUsage', fakeSettings.getDataUsageForTest());
    subpage.set('settings.visibility', fakeSettings.getVisibilityForTest());
    subpage.set(
        'settings.allowedContacts', fakeSettings.getAllowedContactsForTest());
    subpage.set(
        'settings.isOnboardingComplete', fakeSettings.isOnboardingComplete());
  }

  function createSubpage(is_enabled, is_onboarding_complete) {
    PolymerTest.clearBody();

    subpage = document.createElement('settings-nearby-share-subpage');
    subpage.prefs = {
      'nearby_sharing': {
        'enabled': {
          value: is_enabled,
        },
        'data_usage': {
          value: 3,
        },
        'device_name': {
          value: '',
        },
        'onboarding_complete': {
          value: is_onboarding_complete,
        },
      }
    };
    subpage.isSettingsRetreived = true;

    document.body.appendChild(subpage);
    Polymer.dom.flush();
  }

  // Returns true if the element exists and has not been 'removed' by the
  // Polymer template system.
  function doesElementExist(selector) {
    const el = subpage.$$(selector);
    return (el !== null) && (el.style.display !== 'none');
  }

  function subpageControlsHidden(is_hidden) {
    assertEquals(is_hidden, !doesElementExist('#highVisibilityToggle'));
    assertEquals(is_hidden, !doesElementExist('#editDeviceNameButton'));
    assertEquals(is_hidden, !doesElementExist('#editVisibilityButton'));
    assertEquals(is_hidden, !doesElementExist('#editDataUsageButton'));
  }

  function subpageControlsDisabled(is_disabled) {
    assertEquals(
        is_disabled,
        subpage.$$('#highVisibilityToggle').hasAttribute('disabled'));
    assertEquals(
        is_disabled,
        subpage.$$('#editDeviceNameButton').hasAttribute('disabled'));
    assertEquals(
        is_disabled,
        subpage.$$('#editVisibilityButton').hasAttribute('disabled'));
    assertEquals(
        is_disabled,
        subpage.$$('#editDataUsageButton').hasAttribute('disabled'));
  }

  function flushAsync() {
    Polymer.dom.flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  test('feature toggle button controls preference', function() {
    // Ensure that these controls are enabled/disabled when the Nearby is
    // enabled/disabled.
    assertEquals(true, featureToggleButton.checked);
    assertEquals(true, subpage.prefs.nearby_sharing.enabled.value);
    assertEquals('On', featureToggleButton.label.trim());
    subpageControlsHidden(false);
    subpageControlsDisabled(false);

    featureToggleButton.click();
    Polymer.dom.flush();

    assertEquals(false, featureToggleButton.checked);
    assertEquals(false, subpage.prefs.nearby_sharing.enabled.value);
    assertEquals('Off', featureToggleButton.label.trim());
    if (loadTimeData.getValue('isNearbyShareBackgroundScanningEnabled')) {
      subpageControlsHidden(false);
    } else {
      subpageControlsHidden(true);
    }
  });

  test('toggle row controls preference', function() {
    assertEquals(true, featureToggleButton.checked);
    assertEquals(true, subpage.prefs.nearby_sharing.enabled.value);
    assertEquals('On', featureToggleButton.label.trim());

    featureToggleButton.click();

    assertEquals(false, featureToggleButton.checked);
    assertEquals(false, subpage.prefs.nearby_sharing.enabled.value);
    assertEquals('Off', featureToggleButton.label.trim());
  });

  suite('Deeplinking', () => {
    suiteSetup(function() {
      loadTimeData.overrideValues({
        isNearbyShareBackgroundScanningEnabled: true,
      });
    });

    const deepLinkTestData = [
      {settingId: '208', deepLinkElement: '#featureToggleButton'},
      {settingId: '214', deepLinkElement: '#editDeviceNameButton'},
      {settingId: '215', deepLinkElement: '#editVisibilityButton'},
      {settingId: '216', deepLinkElement: '#manageContactsLinkRow'},
      {settingId: '217', deepLinkElement: '#editDataUsageButton'},
      {settingId: '220', deepLinkElement: '#fastInitiationNotificationToggle'},
    ];

    deepLinkTestData.forEach((testData) => {
      test(
          'Deep link to nearby setting element ' + testData.deepLinkElement,
          async () => {
            const params = new URLSearchParams;
            params.append('settingId', testData.settingId);
            settings.Router.getInstance().navigateTo(
                settings.routes.NEARBY_SHARE, params);

            Polymer.dom.flush();

            const deepLinkElement = subpage.$$(testData.deepLinkElement);
            await test_util.waitAfterNextRender(deepLinkElement);
            assertEquals(
                deepLinkElement, subpage.shadowRoot.activeElement,
                'Nearby share setting element ' + testData.deepLinkElement +
                    ' should be focused for settingId=' + testData.settingId);
          });
    });
  });

  test('update device name preference', function() {
    assertEquals('', subpage.prefs.nearby_sharing.device_name.value);

    subpage.$$('#editDeviceNameButton').click();
    Polymer.dom.flush();

    const dialog = subpage.$$('nearby-share-device-name-dialog');
    const oldName = subpage.settings.deviceName;
    const newName = 'NEW NAME';
    dialog.$$('cr-input').value = newName;
    dialog.$$('.action-button').click();
    Polymer.dom.flush();
    syncFakeSettings();
    Polymer.dom.flush();

    assertEquals(newName, subpage.settings.deviceName);
    subpage.set('settings.deviceName', oldName);
  });

  test('validate device name preference', async () => {
    subpage.$$('#editDeviceNameButton').click();
    Polymer.dom.flush();
    const dialog = subpage.$$('nearby-share-device-name-dialog');
    const input = dialog.$$('cr-input');
    const doneButton = dialog.$$('#doneButton');

    fakeSettings.setNextDeviceNameResult(
        nearbyShare.mojom.DeviceNameValidationResult.kErrorEmpty);
    input.fire('input');
    // Allow the validation promise to resolve.
    await test_util.waitAfterNextRender();
    Polymer.dom.flush();
    assertTrue(input.invalid);
    assertTrue(doneButton.disabled);

    fakeSettings.setNextDeviceNameResult(
        nearbyShare.mojom.DeviceNameValidationResult.kValid);
    input.fire('input');
    await test_util.waitAfterNextRender();
    Polymer.dom.flush();
    assertFalse(input.invalid);
    assertFalse(doneButton.disabled);
  });

  test('update data usage preference', function() {
    assertEquals(3, subpage.prefs.nearby_sharing.data_usage.value);

    subpage.$$('#editDataUsageButton').click();
    Polymer.dom.flush();

    const dialog = subpage.$$('nearby-share-data-usage-dialog');
    dialog.$$('#dataUsageDataButton').click();
    dialog.$$('.action-button').click();

    assertEquals(2, subpage.prefs.nearby_sharing.data_usage.value);
  });

  test('update visibility shows dialog', function() {
    // NOTE: all value editing is done and tested in the
    // nearby-contact-visibility component which is hosted directly on the
    // dialog. Here we just verify the dialog shows up, it has the component,
    // and it has a close/action button.
    subpage.$$('#editVisibilityButton').click();
    Polymer.dom.flush();

    const dialog = subpage.$$('nearby-share-contact-visibility-dialog');
    assertTrue(dialog.$$('nearby-contact-visibility') !== null);
    dialog.$$('.action-button').click();
  });

  test('toggle high visibility from UI', async function() {
    subpage.$$('#highVisibilityToggle').click();
    Polymer.dom.flush();
    assertTrue(fakeReceiveManager.getInHighVisibilityForTest());

    const dialog = subpage.$$('nearby-share-receive-dialog');
    assertTrue(!!dialog);

    await test_util.waitAfterNextRender(dialog);
    const highVisibilityDialog = dialog.$$('nearby-share-high-visibility-page');
    assertTrue(test_util.isVisible(highVisibilityDialog));

    dialog.close_();
    assertFalse(fakeReceiveManager.getInHighVisibilityForTest());
  });

  test(
      'high visibility UI updates from high visibility changes',
      async function() {
        const highVisibilityToggle = subpage.$$('#highVisibilityToggle');
        assertFalse(highVisibilityToggle.checked);

        fakeReceiveManager.setInHighVisibilityForTest(true);
        assertTrue(highVisibilityToggle.checked);

        fakeReceiveManager.setInHighVisibilityForTest(false);
        assertFalse(highVisibilityToggle.checked);

        // Process stopped unchecks the toggle.
        fakeReceiveManager.setInHighVisibilityForTest(true);
        assertTrue(highVisibilityToggle.checked);
        subpage.onNearbyProcessStopped();
        Polymer.dom.flush();
        assertFalse(highVisibilityToggle.checked);

        // Failure to start advertising unchecks the toggle.
        fakeReceiveManager.setInHighVisibilityForTest(false);
        fakeReceiveManager.setInHighVisibilityForTest(true);
        assertTrue(highVisibilityToggle.checked);
        subpage.onStartAdvertisingFailure();
        Polymer.dom.flush();
        assertFalse(highVisibilityToggle.checked);

        // Toggle still gets unchecked even if advertising was not attempted.
        // E.g. if Bluetooth is off when high visibility is toggled.
        fakeReceiveManager.setInHighVisibilityForTest(false);
        subpage.inHighVisibility_ = true;
        subpage.showHighVisibilityPage_();
        const dialog = subpage.$$('nearby-share-receive-dialog');
        assertTrue(!!dialog);
        await test_util.waitAfterNextRender(dialog);
        const highVisibilityDialog =
            dialog.$$('nearby-share-high-visibility-page');
        await test_util.waitAfterNextRender(dialog);
        assertTrue(test_util.isVisible(highVisibilityDialog));
        highVisibilityDialog.registerResult =
            nearbyShare.mojom.RegisterReceiveSurfaceResult.kNoConnectionMedium;
        await test_util.waitAfterNextRender(highVisibilityDialog);
        highVisibilityDialog.$$('nearby-page-template')
            .$$('#closeButton')
            .click();
        Polymer.dom.flush();
        assertFalse(highVisibilityToggle.checked);
      });

  test('GAIA email, account manager enabled', async () => {
    await accountManagerBrowserProxy.whenCalled('getAccounts');
    Polymer.dom.flush();

    const profileName = subpage.$$('#profileName');
    assertEquals('Primary Account', profileName.textContent.trim());
    const profileLabel = subpage.$$('#profileLabel');
    assertEquals('primary@gmail.com', profileLabel.textContent.trim());
  });

  test('show receive dialog', function() {
    subpage.showReceiveDialog_ = true;
    Polymer.dom.flush();

    const dialog = subpage.$$('nearby-share-receive-dialog');
    assertTrue(!!dialog);
  });

  test('show high visibility dialog', async function() {
    // Mock performance.now to return a constant 0 for testing.
    const originalNow = performance.now;
    performance.now = () => {
      return 0;
    };

    const params = new URLSearchParams;
    params.append('receive', '1');
    params.append('timeout', '600');  // 10 minutes
    settings.Router.getInstance().navigateTo(
        settings.routes.NEARBY_SHARE, params);

    const dialog = subpage.$$('nearby-share-receive-dialog');
    assertTrue(!!dialog);
    const highVisibilityDialog = dialog.$$('nearby-share-high-visibility-page');
    assertTrue(!!highVisibilityDialog);
    assertFalse(highVisibilityDialog.highVisibilityTimedOut_());

    Polymer.dom.flush();
    await test_util.waitAfterNextRender(dialog);

    assertTrue(test_util.isVisible(highVisibilityDialog));
    assertEquals(highVisibilityDialog.shutoffTimestamp, 600 * 1000);

    // Restore mock
    performance.now = originalNow;
  });

  test('high visibility dialog times out', async function() {
    // Mock performance.now to return a constant 0 for testing.
    const originalNow = performance.now;
    performance.now = () => {
      return 0;
    };

    const params = new URLSearchParams;
    params.append('receive', '1');
    params.append('timeout', '600');  // 10 minutes
    settings.Router.getInstance().navigateTo(
        settings.routes.NEARBY_SHARE, params);

    const dialog = subpage.$$('nearby-share-receive-dialog');
    assertTrue(!!dialog);
    const highVisibilityDialog = dialog.$$('nearby-share-high-visibility-page');
    assertTrue(!!highVisibilityDialog);

    highVisibilityDialog.calculateRemainingTime_();
    assertFalse(highVisibilityDialog.highVisibilityTimedOut_());

    // Set time past the shutoffTime.
    performance.now = () => {
      return 6000001;
    };

    highVisibilityDialog.calculateRemainingTime_();
    await test_util.waitAfterNextRender(dialog);
    assertTrue(test_util.isVisible(highVisibilityDialog));
    assertTrue(highVisibilityDialog.highVisibilityTimedOut_());

    // Restore mock
    performance.now = originalNow;
  });

  test('download contacts on attach', async () => {
    await flushAsync();
    // Ensure contacts download occurs when the subpage is attached.
    assertTrue(fakeContactManager.downloadContactsCalled);
  });

  test('Do not download contacts on attach pre-onboarding', async () => {
    await flushAsync();

    subpage.remove();
    settings.Router.getInstance().resetRouteForTesting();

    setupFakes();
    fakeSettings.setEnabled(false);
    fakeSettings.setIsOnboardingComplete(false);
    syncFakeSettings();
    createSubpage(/*is_enabled=*/ false, /*is_onboarding_complete=*/ false);

    await flushAsync();
    // Ensure contacts download occurs when the subpage is attached.
    assertFalse(fakeContactManager.downloadContactsCalled);
  });

  test('Show setup button pre-onboarding', async () => {
    await flushAsync();

    subpage.remove();
    settings.Router.getInstance().resetRouteForTesting();

    setupFakes();
    createSubpage(/*is_enabled=*/ false, /*is_onboarding_complete=*/ false);

    await flushAsync();
    assertFalse(doesElementExist('#featureToggleButton'));
    assertTrue(doesElementExist('#setupRow'));

    // Clicking starts onboarding flow
    subpage.$$('#setupRow').querySelector('cr-button').click();
    await flushAsync();
    assertTrue(doesElementExist('#receiveDialog'));
    assertEquals(
        'active', subpage.$$('#receiveDialog').$$('#onboarding').className);
  });

  test('feature toggle UI changes', function() {
    // Ensure toggle off UI occurs when toggle off.
    assertEquals(true, featureToggleButton.checked);
    assertEquals('On', featureToggleButton.label.trim());
    assertTrue(featureToggleButton.classList.contains('enabled-toggle-on'));
    assertFalse(featureToggleButton.classList.contains('enabled-toggle-off'));

    featureToggleButton.click();

    assertEquals(false, featureToggleButton.checked);
    assertEquals('Off', featureToggleButton.label.trim());
    assertFalse(featureToggleButton.classList.contains('enabled-toggle-on'));
    assertTrue(featureToggleButton.classList.contains('enabled-toggle-off'));
  });

  test('subpage hidden when feature toggled off', function() {
    // Ensure that the subpage content is hidden when the Nearby is off.
    const subpageContent = subpage.$$('#subpageContent');
    const highVizToggle = subpage.$$('#highVisibilityToggle');
    const editDeviceNameButton = subpage.$$('#editDeviceNameButton');
    const editVisibilityButton = subpage.$$('#editVisibilityButton');
    const editDataUsageButton = subpage.$$('#editDataUsageButton');

    assertEquals(true, featureToggleButton.checked);
    assertEquals(true, subpage.prefs.nearby_sharing.enabled.value);
    assertEquals('On', featureToggleButton.label.trim());
    assertTrue(doesElementExist('#help'));

    editVisibilityButton.click();
    Polymer.dom.flush();
    const visibilityDialog =
        subpage.$$('nearby-share-contact-visibility-dialog');
    assertTrue(!!visibilityDialog);
    assertTrue(visibilityDialog.$$('nearby-contact-visibility') !== null);

    editDeviceNameButton.click();
    Polymer.dom.flush();
    const deviceNameDialog = subpage.$$('nearby-share-device-name-dialog');
    assertTrue(!!deviceNameDialog);

    editDataUsageButton.click();
    Polymer.dom.flush();
    const dataUsageDialog = subpage.$$('nearby-share-data-usage-dialog');
    assertTrue(!!dataUsageDialog);

    highVizToggle.click();
    Polymer.dom.flush();
    const receiveDialog = subpage.$$('nearby-share-receive-dialog');
    assertTrue(!!receiveDialog);

    featureToggleButton.click();
    Polymer.dom.flush();

    assertEquals(false, featureToggleButton.checked);
    assertEquals(false, subpage.prefs.nearby_sharing.enabled.value);
    assertEquals('Off', featureToggleButton.label.trim());
    assertEquals('none', subpageContent.style.display);
    assertEquals('none', subpage.$$('#helpContent').style.display);
    if (loadTimeData.getValue('isNearbyShareBackgroundScanningEnabled')) {
      subpageControlsHidden(false);
    } else {
      subpageControlsHidden(true);
    }
    assertFalse(doesElementExist('#help'));
  });

  test('Fast init toggle doesn\'t exist', function() {
    if (loadTimeData.getValue('isNearbyShareBackgroundScanningEnabled')) {
      assertTrue(!!subpage.$$('#fastInitiationNotificationToggle'));
    } else {
      assertFalse(!!subpage.$$('#fastInitiationNotificationToggle'));
    }
  });

  suite('Background Scanning Enabled', function() {
    suiteSetup(function() {
      loadTimeData.overrideValues({
        isNearbyShareBackgroundScanningEnabled: true,
      });
    });

    test('UX changes disabled when no hardware support', async () => {
      subpage.set('settings.isFastInitiationHardwareSupported', false);
      await flushAsync();

      // Toggle doesnt exist
      const fastInitToggle = subpage.$$('#fastInitiationNotificationToggle');
      assertFalse(!!fastInitToggle);

      // Subpage contents do not show when feature off
      featureToggleButton.click();
      Polymer.dom.flush();

      assertFalse(featureToggleButton.checked);
      assertFalse(subpage.prefs.nearby_sharing.enabled.value);
      assertEquals('Off', featureToggleButton.label.trim());

      subpageControlsHidden(true);
    });


    test('Fast initiation notification toggle', async () => {
      const fastInitToggle = subpage.$$('#fastInitiationNotificationToggle');
      assertTrue(!!fastInitToggle);
      await flushAsync();
      assertTrue(fastInitToggle.checked);
      assertEquals(
          nearbyShare.mojom.FastInitiationNotificationState.kEnabled,
          subpage.settings.fastInitiationNotificationState);

      fastInitToggle.click();
      await flushAsync();
      assertFalse(fastInitToggle.checked);
      assertEquals(
          nearbyShare.mojom.FastInitiationNotificationState.kDisabledByUser,
          subpage.settings.fastInitiationNotificationState);
    });

    test('Subpage content visible but disabled when feature off', async () => {
      featureToggleButton.click();
      Polymer.dom.flush();

      assertFalse(featureToggleButton.checked);
      assertFalse(subpage.prefs.nearby_sharing.enabled.value);
      assertEquals('Off', featureToggleButton.label.trim());

      subpageControlsHidden(false);
      subpageControlsDisabled(true);
    });

    test('Subpage content not visible pre-onboarding', async () => {
      featureToggleButton.click();
      subpage.set('prefs.nearby_sharing.onboarding_complete.value', false);
      await flushAsync();

      assertFalse(subpage.prefs.nearby_sharing.enabled.value);
      subpageControlsHidden(true);
    });

    test('Subpage content visible but disabled when feature off', async () => {
      featureToggleButton.click();
      Polymer.dom.flush();

      assertEquals(false, featureToggleButton.checked);
      assertEquals(false, subpage.prefs.nearby_sharing.enabled.value);
      assertEquals('Off', featureToggleButton.label.trim());

      subpageControlsHidden(false);
      subpageControlsDisabled(true);
    });

    test('Subpage content not visible pre-onboarding', async () => {
      featureToggleButton.click();
      subpage.set('prefs.nearby_sharing.onboarding_complete.value', false);
      await flushAsync();

      assertEquals(false, subpage.prefs.nearby_sharing.enabled.value);
      subpageControlsHidden(true);
    });
  });
});
