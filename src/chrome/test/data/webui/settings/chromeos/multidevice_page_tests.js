// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MultiDeviceBrowserProxyImpl, MultiDeviceFeature, MultiDeviceFeatureState, MultiDevicePageContentData, MultiDeviceSettingsMode, PhoneHubFeatureAccessStatus, Router, routes, setContactManagerForTesting, setNearbyShareSettingsForTesting} from 'chrome://os-settings/chromeos/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {isChildVisible, waitAfterNextRender} from 'chrome://test/test_util.js';

import {assertEquals, assertFalse, assertNotEquals, assertTrue} from '../../chai_assert.js';
import {FakeContactManager} from '../../nearby_share/shared/fake_nearby_contact_manager.m.js';
import {FakeNearbyShareSettings} from '../../nearby_share/shared/fake_nearby_share_settings.m.js';

import {createFakePageContentData, HOST_DEVICE, TestMultideviceBrowserProxy} from './test_multidevice_browser_proxy.js';

suite('Multidevice', function() {
  let multidevicePage = null;
  let browserProxy = null;
  let ALL_MODES;
  /** @type {!FakeContactManager} */
  let fakeContactManager = null;
  /** @type {!FakeNearbyShareSettings} */
  let fakeSettings = null;

  /**
   * Sets pageContentData via WebUI Listener and flushes.
   * @param {!MultiDevicePageContentData}
   */
  function setPageContentData(newPageContentData) {
    cr.webUIListenerCallback(
        'settings.updateMultidevicePageContentData', newPageContentData);
    flush();
  }

  function flushAsync() {
    flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  /**
   * Sets pageContentData to the specified mode. If it is a mode corresponding
   * to a set host, it will set the hostDeviceName to the provided name or else
   * default to HOST_DEVICE.
   * @param {MultiDeviceSettingsMode} newMode
   * @param {string=} opt_newHostDeviceName Overrides default if |newMode|
   *     corresponds to a set host.
   */
  function setHostData(newMode, opt_newHostDeviceName) {
    setPageContentData(
        createFakePageContentData(newMode, opt_newHostDeviceName));
  }

  /**
   * @param {MultiDeviceFeatureState} newState
   */
  function setSuiteState(newState) {
    setPageContentData(Object.assign(
        {}, multidevicePage.pageContentData, {betterTogetherState: newState}));
  }

  /**
   * @param {MultiDeviceFeatureState} newState
   */
  function setSmartLockState(newState) {
    setPageContentData(Object.assign(
        {}, multidevicePage.pageContentData, {smartLockState: newState}));
  }

  /**
   * @param {MultiDeviceFeatureState} newState
   */
  function setPhoneHubNotificationsState(newState) {
    setPageContentData(Object.assign(
        {}, multidevicePage.pageContentData,
        {phoneHubNotificationsState: newState}));
  }

  /**
   * @param {boolean} accessGranted
   */
  function setPhoneHubNotificationAccessGranted(accessGranted) {
    const accessState = accessGranted ?
        PhoneHubFeatureAccessStatus.ACCESS_GRANTED :
        PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED;
    setPageContentData(Object.assign(
        {}, multidevicePage.pageContentData,
        {notificationAccessStatus: accessState}));
  }

  /**
   * @param {boolean} isOnboardingComplete
   */
  function setNearbyShareIsOnboardingComplete(isOnboardingComplete) {
    multidevicePage.setPrefValue(
        'nearby_sharing.onboarding_complete', isOnboardingComplete);
    flush();
  }

  /**
   * @param {boolean} isOnboardingComplete
   */
  function setNearbyShareEnabled(enabled) {
    multidevicePage.setPrefValue('nearby_sharing.enabled', enabled);
    flush();
  }

  /**
   * @param {boolean} isDisallowedByPolicy
   */
  function setNearbyShareDisallowedByPolicy(isDisallowedByPolicy) {
    setPageContentData(Object.assign(
        {}, multidevicePage.pageContentData,
        {isNearbyShareDisallowedByPolicy: isDisallowedByPolicy}));
  }

  /**
   * @param {boolean} isPhoneHubPermissionsDialogSupported
   */
  function setPhoneHubPermissionsDialogSupported(enabled) {
    setPageContentData(Object.assign(
        {}, multidevicePage.pageContentData,
        {isPhoneHubPermissionsDialogSupported: enabled}));
  }

  /**
   * @param {!MultiDeviceFeature} feature The feature to change.
   * @param {boolean} enabled Whether to enable or disable the feature.
   * @param {boolean} authRequired Whether authentication is required for the
   *     change.
   * @return {!Promise} Promise which resolves when the state change has been
   *     verified.
   * @private
   */
  function simulateFeatureStateChangeRequest(feature, enabled, authRequired) {
    // When the user requets a feature state change, an event with the relevant
    // details is handled.
    multidevicePage.fire(
        'feature-toggle-clicked', {feature: feature, enabled: enabled});
    flush();

    if (authRequired) {
      assertTrue(multidevicePage.showPasswordPromptDialog_);
      // Simulate the user entering a valid password, then closing the dialog.
      multidevicePage.$$('#multidevicePasswordPrompt')
          .dispatchEvent(new CustomEvent('token-obtained', {
            bubbles: true,
            composed: true,
            detail: {token: 'validAuthToken', lifetimeSeconds: 300}
          }));
      // Simulate closing the password prompt dialog
      multidevicePage.$$('#multidevicePasswordPrompt')
          .dispatchEvent(
              new CustomEvent('close', {bubbles: true, composed: true}));
      flush();
    }

    if (enabled && feature === MultiDeviceFeature.PHONE_HUB_NOTIFICATIONS) {
      const accessDialog =
          multidevicePage.pageContentData.isPhoneHubPermissionsDialogSupported ?
          multidevicePage.$$('settings-multidevice-permissions-setup-dialog') :
          multidevicePage.$$(
              'settings-multidevice-notification-access-setup-dialog');
      assertEquals(
          !!accessDialog,
          multidevicePage.pageContentData.notificationAccessStatus ===
              PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED);
      return;
    }

    assertFalse(multidevicePage.showPasswordPromptDialog_);
    return browserProxy.whenCalled('setFeatureEnabledState').then(params => {
      assertEquals(feature, params[0]);
      assertEquals(enabled, params[1]);

      // Reset the resolver so that setFeatureEnabledState() can be called
      // multiple times in a test.
      browserProxy.resetResolver('setFeatureEnabledState');
    });
  }

  suiteSetup(function() {
    ALL_MODES = Object.values(MultiDeviceSettingsMode);
  });

  setup(function() {
    fakeContactManager = new FakeContactManager();
    setContactManagerForTesting(fakeContactManager);
    fakeContactManager.setupContactRecords();

    fakeSettings = new FakeNearbyShareSettings();
    fakeSettings.setEnabled(true);
    setNearbyShareSettingsForTesting(fakeSettings);

    PolymerTest.clearBody();
    browserProxy = new TestMultideviceBrowserProxy();
    MultiDeviceBrowserProxyImpl.instance_ = browserProxy;

    loadTimeData.overrideValues({
      isNearbyShareSupported: true,
    });

    multidevicePage = document.createElement('settings-multidevice-page');
    assertTrue(!!multidevicePage);

    multidevicePage.prefs = {
      'nearby_sharing': {
        'onboarding_complete': {
          value: false,
        },
        'enabled': {
          value: false,
        },
      },
    };

    document.body.appendChild(multidevicePage);
    flush();

    return browserProxy.whenCalled('getPageContentData');
  });

  teardown(function() {
    multidevicePage.remove();
    Router.getInstance().resetRouteForTesting();
  });

  const getLabel = () => {
    return multidevicePage.$$('#multidevice-label').textContent.trim();
  };

  const getSubpage = () => multidevicePage.$$('settings-multidevice-subpage');

  test('clicking setup shows multidevice setup dialog', function() {
    setHostData(MultiDeviceSettingsMode.NO_HOST_SET);
    const button = multidevicePage.$$('cr-button');
    assertTrue(!!button);
    button.click();
    return browserProxy.whenCalled('showMultiDeviceSetupDialog');
  });

  test('Deep link to multidevice setup', async () => {
    setHostData(MultiDeviceSettingsMode.NO_HOST_SET);

    const params = new URLSearchParams();
    params.append('settingId', '200');
    Router.getInstance().navigateTo(routes.MULTIDEVICE, params);

    flush();

    const deepLinkElement = multidevicePage.$$('cr-button');
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Setup multidevice button should be focused for settingId=200.');
  });

  test('Open notification access setup dialog route param', async () => {
    Router.getInstance().navigateTo(
        routes.MULTIDEVICE_FEATURES,
        new URLSearchParams('showPhonePermissionSetupDialog=true'));

    PolymerTest.clearBody();
    browserProxy = new TestMultideviceBrowserProxy();
    MultiDeviceBrowserProxyImpl.instance_ = browserProxy;
    browserProxy.data.notificationAccessStatus =
        PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED;

    multidevicePage = document.createElement('settings-multidevice-page');
    assertTrue(!!multidevicePage);

    multidevicePage.prefs = {
      'nearby_sharing': {
        'onboarding_complete': {
          value: false,
        },
        'enabled': {
          value: false,
        },
      },
    };

    document.body.appendChild(multidevicePage);
    await browserProxy.whenCalled('getPageContentData');

    flush();
    assertTrue(!!multidevicePage.$$(
        'settings-multidevice-notification-access-setup-dialog'));

    // Close the dialog.
    multidevicePage.showPhonePermissionSetupDialog_ = false;
    flush();

    // A change in pageContentData will not cause the notification access
    // setup dialog to reappaear
    setPageContentData({});
    flush();

    assertFalse(!!multidevicePage.$$(
        'settings-multidevice-notification-access-setup-dialog'));
  });

  test('Open multidevice permissions setup dialog route param', async () => {
    Router.getInstance().navigateTo(
        routes.MULTIDEVICE_FEATURES,
        new URLSearchParams('showPhonePermissionSetupDialog&mode=1'));

    PolymerTest.clearBody();
    browserProxy = new TestMultideviceBrowserProxy();
    MultiDeviceBrowserProxyImpl.instance_ = browserProxy;
    browserProxy.data.notificationAccessStatus =
        PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED;
    browserProxy.data.isPhoneHubPermissionsDialogSupported = true;

    multidevicePage = document.createElement('settings-multidevice-page');
    assertTrue(!!multidevicePage);

    multidevicePage.prefs = {
      'nearby_sharing': {
        'onboarding_complete': {
          value: false,
        },
        'enabled': {
          value: false,
        },
      },
    };

    document.body.appendChild(multidevicePage);
    await browserProxy.whenCalled('getPageContentData');

    flush();
    assertTrue(
        !!multidevicePage.$$('settings-multidevice-permissions-setup-dialog'));

    // Close the dialog.
    multidevicePage.showPhonePermissionSetupDialog_ = false;
    flush();

    // A change in pageContentData will not cause the multidevice permissions
    // setup dialog to reappaear.
    setPageContentData({});
    flush();

    assertFalse(
        !!multidevicePage.$$('settings-multidevice-permissions-setup-dialog'));
  });

  test('headings render based on mode and host', function() {
    for (const mode of ALL_MODES) {
      setHostData(mode);
      assertEquals(multidevicePage.isHostSet(), getLabel() === HOST_DEVICE);
    }
  });

  test('changing host device changes header', function() {
    setHostData(MultiDeviceSettingsMode.HOST_SET_VERIFIED);
    assertEquals(getLabel(), HOST_DEVICE);
    const anotherHost = 'Super Duper ' + HOST_DEVICE;
    setHostData(MultiDeviceSettingsMode.HOST_SET_VERIFIED, anotherHost);
    assertEquals(getLabel(), anotherHost);
  });

  test('item is actionable if and only if a host is set', function() {
    for (const mode of ALL_MODES) {
      setHostData(mode);
      assertEquals(
          multidevicePage.isHostSet(),
          !!multidevicePage.$$('#suiteLinkWrapper').hasAttribute('actionable'));
    }
  });

  test(
      'clicking item with verified host opens subpage with features',
      function() {
        setHostData(MultiDeviceSettingsMode.HOST_SET_VERIFIED);
        assertFalse(!!getSubpage());
        multidevicePage.$$('#suiteLinkWrapper').click();
        assertTrue(!!getSubpage());
        assertTrue(!!getSubpage().$$('settings-multidevice-feature-item'));
      });

  test(
      'clicking item with unverified set host opens subpage without features',
      function() {
        setHostData(
            MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_VERIFICATION,
            HOST_DEVICE);
        assertFalse(!!getSubpage());
        multidevicePage.$$('#suiteLinkWrapper').click();
        assertTrue(!!getSubpage());
        assertFalse(!!getSubpage().$$('settings-multidevice-feature-item'));
      });

  test('policy prohibited suite shows policy indicator', function() {
    setHostData(MultiDeviceSettingsMode.NO_ELIGIBLE_HOSTS);
    assertFalse(!!multidevicePage.$$('#suitePolicyIndicator'));
    // Prohibit suite by policy.
    setSuiteState(MultiDeviceFeatureState.PROHIBITED_BY_POLICY);
    assertTrue(!!multidevicePage.$$('#suitePolicyIndicator'));
    // Reallow suite.
    setSuiteState(MultiDeviceFeatureState.DISABLED_BY_USER);
    assertFalse(!!multidevicePage.$$('#suitePolicyIndicator'));
  });

  test('Multidevice permissions setup dialog', () => {
    setPhoneHubNotificationsState(MultiDeviceFeatureState.DISABLED_BY_USER);
    assertFalse(!!multidevicePage.$$(
        'settings-multidevice-notification-access-setup-dialog'));

    setPhoneHubNotificationAccessGranted(false);
    simulateFeatureStateChangeRequest(
        MultiDeviceFeature.PHONE_HUB_NOTIFICATIONS,
        /*enabled=*/ true, /*authRequired=*/ false);

    // Close the dialog.
    multidevicePage.showPhonePermissionSetupDialog_ = false;

    setPhoneHubNotificationAccessGranted(false);
    simulateFeatureStateChangeRequest(
        MultiDeviceFeature.PHONE_HUB_NOTIFICATIONS,
        /*enabled=*/ false, /*authRequired=*/ false);

    setPhoneHubNotificationAccessGranted(true);
    simulateFeatureStateChangeRequest(
        MultiDeviceFeature.PHONE_HUB_NOTIFICATIONS,
        /*enabled=*/ true, /*authRequired=*/ false);

    multidevicePage.pageContentData.isNotificationAccessGranted = true;
    simulateFeatureStateChangeRequest(
        MultiDeviceFeature.PHONE_HUB_NOTIFICATIONS,
        /*enabled=*/ false, /*authRequired=*/ false);
  });

  test('New multidevice permissions setup dialog', () => {
    setPhoneHubPermissionsDialogSupported(true);
    setPhoneHubNotificationsState(MultiDeviceFeatureState.DISABLED_BY_USER);
    assertFalse(
        !!multidevicePage.$$('settings-multidevice-permissions-setup-dialog'));

    setPhoneHubNotificationAccessGranted(false);
    simulateFeatureStateChangeRequest(
        MultiDeviceFeature.PHONE_HUB_NOTIFICATIONS,
        /*enabled=*/ true, /*authRequired=*/ false);

    // Close the dialog.
    multidevicePage.showPhonePermissionSetupDialog_ = false;

    setPhoneHubNotificationAccessGranted(false);
    simulateFeatureStateChangeRequest(
        MultiDeviceFeature.PHONE_HUB_NOTIFICATIONS,
        /*enabled=*/ false, /*authRequired=*/ false);

    setPhoneHubNotificationAccessGranted(true);
    simulateFeatureStateChangeRequest(
        MultiDeviceFeature.PHONE_HUB_NOTIFICATIONS,
        /*enabled=*/ true, /*authRequired=*/ false);

    multidevicePage.pageContentData.isNotificationAccessGranted = true;
    simulateFeatureStateChangeRequest(
        MultiDeviceFeature.PHONE_HUB_NOTIFICATIONS,
        /*enabled=*/ false, /*authRequired=*/ false);
  });

  test('Disabling features never requires authentication', () => {
    const Feature = MultiDeviceFeature;

    const disableFeatureFn = feature => {
      return simulateFeatureStateChangeRequest(
          feature, false /* enabled */, false /* authRequired */);
    };

    return disableFeatureFn(Feature.BETTER_TOGETHER_SUITE)
        .then(() => {
          return disableFeatureFn(Feature.INSTANT_TETHERING);
        })
        .then(() => {
          return disableFeatureFn(Feature.MESSAGES);
        })
        .then(() => {
          return disableFeatureFn(Feature.SMART_LOCK);
        });
  });

  test('Enabling some features requires authentication; others do not', () => {
    const Feature = MultiDeviceFeature;
    const FeatureState = MultiDeviceFeatureState;

    const enableFeatureWithoutAuthFn = feature => {
      return simulateFeatureStateChangeRequest(
          feature, true /* enabled */, false /* authRequired */);
    };
    const enableFeatureWithAuthFn = feature => {
      return simulateFeatureStateChangeRequest(
          feature, true /* enabled */, true /* authRequired */);
    };

    // Start out with SmartLock being disabled by the user. This means that
    // the first attempt to enable BETTER_TOGETHER_SUITE below will not
    // require authentication.
    setSmartLockState(FeatureState.DISABLED_BY_USER);

    // INSTANT_TETHERING requires no authentication.
    return enableFeatureWithoutAuthFn(Feature.INSTANT_TETHERING)
        .then(() => {
          // MESSAGES requires no authentication.
          return enableFeatureWithoutAuthFn(Feature.MESSAGES);
        })
        .then(() => {
          // BETTER_TOGETHER_SUITE requires no authentication normally.
          return enableFeatureWithoutAuthFn(Feature.BETTER_TOGETHER_SUITE);
        })
        .then(() => {
          // BETTER_TOGETHER_SUITE requires authentication when SmartLock's
          // state is UNAVAILABLE_SUITE_DISABLED.
          setSmartLockState(FeatureState.UNAVAILABLE_SUITE_DISABLED);
          return enableFeatureWithAuthFn(Feature.BETTER_TOGETHER_SUITE);
        })
        .then(() => {
          // BETTER_TOGETHER_SUITE requires authentication when SmartLock's
          // state is UNAVAILABLE_INSUFFICIENT_SECURITY.
          setSmartLockState(FeatureState.UNAVAILABLE_INSUFFICIENT_SECURITY);
          return enableFeatureWithAuthFn(Feature.BETTER_TOGETHER_SUITE);
        })
        .then(() => {
          // SMART_LOCK always requires authentication.
          return enableFeatureWithAuthFn(Feature.SMART_LOCK);
        });
  });

  test('Nearby setup button shown before onboarding is complete', async () => {
    setNearbyShareDisallowedByPolicy(false);
    assertTrue(isChildVisible(
        multidevicePage, '#nearbySetUp', /*checkLightDom=*/ false));
    assertFalse(isChildVisible(
        multidevicePage, '#nearbySharingToggleButton',
        /*checkLightDom=*/ false));

    setNearbyShareIsOnboardingComplete(true);
    assertFalse(isChildVisible(
        multidevicePage, '#nearbySetUp', /*checkLightDom=*/ false));
    assertTrue(isChildVisible(
        multidevicePage, '#nearbySharingToggleButton',
        /*checkLightDom=*/ false));
    assertFalse(multidevicePage.$$('#nearbySharingToggleButton').disabled);
  });

  test('Nearby disabled toggle shown if disallowed by policy', async () => {
    setNearbyShareDisallowedByPolicy(false);
    assertTrue(isChildVisible(
        multidevicePage, '#nearbySetUp', /*checkLightDom=*/ false));
    assertFalse(isChildVisible(
        multidevicePage, '#nearbySharingToggleButton',
        /*checkLightDom=*/ false));

    setNearbyShareDisallowedByPolicy(true);
    assertFalse(isChildVisible(
        multidevicePage, '#nearbySetUp', /*checkLightDom=*/ false));
    assertTrue(isChildVisible(
        multidevicePage, '#nearbySharingToggleButton',
        /*checkLightDom=*/ false));
    assertTrue(multidevicePage.$$('#nearbySharingToggleButton').disabled);
  });

  test('Nearby description shown before onboarding is completed', async () => {
    setNearbyShareDisallowedByPolicy(false);
    assertTrue(isChildVisible(
        multidevicePage, '#nearbyShareSecondary > localized-link',
        /*checkLightDom=*/ false));

    setNearbyShareIsOnboardingComplete(true);
    assertFalse(isChildVisible(
        multidevicePage, '#nearbyShareSecondary > localized-link',
        /*checkLightDom=*/ false));
    assertEquals(
        multidevicePage.$$('#nearbyShareSecondary').textContent.trim(), 'Off');
  });

  test('Nearby description shown if disallowed by policy', async () => {
    setNearbyShareDisallowedByPolicy(false);
    setNearbyShareIsOnboardingComplete(true);
    assertFalse(isChildVisible(
        multidevicePage, '#nearbyShareSecondary > localized-link',
        /*checkLightDom=*/ false));
    assertEquals(
        multidevicePage.$$('#nearbyShareSecondary').textContent.trim(), 'Off');

    setNearbyShareDisallowedByPolicy(true);
    assertTrue(isChildVisible(
        multidevicePage, '#nearbyShareSecondary > localized-link',
        /*checkLightDom=*/ false));
  });

  test('Nearby policy indicator shown when disallowed by policy', async () => {
    setNearbyShareDisallowedByPolicy(false);
    assertFalse(isChildVisible(
        multidevicePage, '#nearbyPolicyIndicator',
        /*checkLightDom=*/ false));

    setNearbyShareDisallowedByPolicy(true);
    assertTrue(isChildVisible(
        multidevicePage, '#nearbyPolicyIndicator',
        /*checkLightDom=*/ false));

    setNearbyShareDisallowedByPolicy(false);
    assertFalse(isChildVisible(
        multidevicePage, '#nearbyPolicyIndicator',
        /*checkLightDom=*/ false));
  });

  test('Nearby subpage not available when disallowed by policy', async () => {
    setNearbyShareDisallowedByPolicy(true);
    assertFalse(
        !!multidevicePage.$$('#nearbyLinkWrapper').hasAttribute('actionable'));

    setNearbyShareDisallowedByPolicy(false);
    assertTrue(
        !!multidevicePage.$$('#nearbyLinkWrapper').hasAttribute('actionable'));
  });

  test('Better Together Suite icon visible when there is no host set', () => {
    setHostData(MultiDeviceSettingsMode.NO_HOST_SET);
    assertTrue(isChildVisible(
        multidevicePage, '#betterTogetherSuiteIcon',
        /*checkLightDom=*/ false));
  });

  test('Better Together Suite icon visible when there is a host set', () => {
    setHostData(MultiDeviceSettingsMode.HOST_SET_VERIFIED);
    assertTrue(isChildVisible(
        multidevicePage, '#betterTogetherSuiteIcon',
        /*checkLightDom=*/ false));
  });

  test('Better Together Suite icon remains visible when host added', () => {
    setHostData(MultiDeviceSettingsMode.NO_HOST_SET);
    assertTrue(isChildVisible(
        multidevicePage, '#betterTogetherSuiteIcon',
        /*checkLightDom=*/ false));

    setHostData(MultiDeviceSettingsMode.HOST_SET_VERIFIED);
    assertTrue(isChildVisible(
        multidevicePage, '#betterTogetherSuiteIcon',
        /*checkLightDom=*/ false));
  });

  test('Better Together Suite icon remains visible when host removed', () => {
    setHostData(MultiDeviceSettingsMode.HOST_SET_VERIFIED);
    assertTrue(isChildVisible(
        multidevicePage, '#betterTogetherSuiteIcon',
        /*checkLightDom=*/ false));

    setHostData(MultiDeviceSettingsMode.NO_HOST_SET);
    assertTrue(isChildVisible(
        multidevicePage, '#betterTogetherSuiteIcon',
        /*checkLightDom=*/ false));
  });

  test(
      'Nearby share sub page arrow is not visible before onboarding',
      async () => {
        setNearbyShareDisallowedByPolicy(false);
        assertTrue(isChildVisible(
            multidevicePage, '#nearbySetUp', /*checkLightDom=*/ false));
        if (loadTimeData.getValue('isNearbyShareBackgroundScanningEnabled')) {
          assertTrue(isChildVisible(
              multidevicePage, '#nearbyShareSubpageArrow',
              /*checkLightDom=*/ false));
        } else {
          assertFalse(isChildVisible(
              multidevicePage, '#nearbyShareSubpageArrow',
              /*checkLightDom=*/ false));
        }

        setNearbyShareIsOnboardingComplete(true);
        setNearbyShareEnabled(true);
        flush();
        assertTrue(isChildVisible(
            multidevicePage, '#nearbyShareSubpageArrow',
            /*checkLightDom=*/ false));
      });

  test(
      'Clicking nearby subpage before onboarding initiates onboarding',
      async () => {
        setNearbyShareDisallowedByPolicy(false);
        assertTrue(isChildVisible(
            multidevicePage, '#nearbySetUp', /*checkLightDom=*/ false));
        if (loadTimeData.getValue('isNearbyShareBackgroundScanningEnabled')) {
          assertTrue(isChildVisible(
              multidevicePage, '#nearbyShareSubpageArrow',
              /*checkLightDom=*/ false));
        } else {
          assertFalse(isChildVisible(
              multidevicePage, '#nearbyShareSubpageArrow',
              /*checkLightDom=*/ false));
        }

        const router = Router.getInstance();
        multidevicePage.$$('#nearbyLinkWrapper').click();
        await flushAsync();
        assertEquals(routes.NEARBY_SHARE, router.getCurrentRoute());
        if (loadTimeData.getValue('isNearbyShareBackgroundScanningEnabled')) {
          assertFalse(router.getQueryParameters().has('onboarding'));
        } else {
          assertTrue(router.getQueryParameters().has('onboarding'));
        }
      });

  test('Clicking nearby subpage after onboarding enters subpage', async () => {
    setNearbyShareDisallowedByPolicy(false);
    assertTrue(isChildVisible(
        multidevicePage, '#nearbySetUp', /*checkLightDom=*/ false));
    if (loadTimeData.getValue('isNearbyShareBackgroundScanningEnabled')) {
      assertTrue(isChildVisible(
          multidevicePage, '#nearbyShareSubpageArrow',
          /*checkLightDom=*/ false));
    } else {
      assertFalse(isChildVisible(
          multidevicePage, '#nearbyShareSubpageArrow',
          /*checkLightDom=*/ false));
    }

    setNearbyShareIsOnboardingComplete(true);
    setNearbyShareEnabled(true);
    flush();

    assertTrue(isChildVisible(
        multidevicePage, '#nearbyShareSubpageArrow',
        /*checkLightDom=*/ false));
    const router = Router.getInstance();
    multidevicePage.$$('#nearbyLinkWrapper').click();
    await flushAsync();
    assertEquals(routes.NEARBY_SHARE, router.getCurrentRoute());
    assertFalse(router.getQueryParameters().has('onboarding'));
  });

  test('Settings mojo changes propagate to settings property', async () => {
    // Allow initial settings to be loaded.
    await flushAsync();

    const newName = 'NEW NAME';
    assertNotEquals(newName, multidevicePage.get('settings.deviceName'));

    await fakeSettings.setDeviceName(newName);
    await flushAsync();
    assertEquals(newName, multidevicePage.get('settings.deviceName'));

    const newEnabledState = !multidevicePage.get('settings.enabled');
    assertNotEquals(newEnabledState, multidevicePage.get('settings.enabled'));

    await fakeSettings.setEnabled(newEnabledState);
    await flushAsync();
    assertEquals(newEnabledState, multidevicePage.get('settings.enabled'));
  });

  suite('Background Scanning Enabled', function() {
    suiteSetup(function() {
      loadTimeData.overrideValues({
        isNearbyShareBackgroundScanningEnabled: true,
      });
    });

    test(
        'Nearby share sub page arrow is visible before onboarding',
        async () => {
          // Arrow only visible if background scanning feature flag is enabled
          // and hardware offloading is supported.
          await flushAsync();
          setNearbyShareDisallowedByPolicy(false);
          multidevicePage.set(
              'settings.isFastInitiationHardwareSupported', true);

          setNearbyShareDisallowedByPolicy(false);
          assertTrue(isChildVisible(
              multidevicePage, '#nearbySetUp', /*checkLightDom=*/ false));
          assertTrue(isChildVisible(
              multidevicePage, '#nearbyShareSubpageArrow',
              /*checkLightDom=*/ false));

          setNearbyShareIsOnboardingComplete(true);
          setNearbyShareEnabled(true);
          await flushAsync();
          assertTrue(isChildVisible(
              multidevicePage, '#nearbyShareSubpageArrow',
              /*checkLightDom=*/ false));
        });

    test('No Background Scanning hardware support', async () => {
      // Ensure initial nearby settings values are set before overriding.
      await flushAsync();
      setNearbyShareDisallowedByPolicy(false);
      multidevicePage.set('settings.isFastInitiationHardwareSupported', false);
      await flushAsync();

      assertTrue(isChildVisible(
          multidevicePage, '#nearbySetUp', /*checkLightDom=*/ false));
      assertFalse(isChildVisible(
          multidevicePage, '#nearbyShareSubpageArrow',
          /*checkLightDom=*/ false));

      // Clicking on Nearby Subpage row should initiate onboarding.
      const router = Router.getInstance();
      assertTrue(!!multidevicePage.$$('#nearbyLinkWrapper'));
      multidevicePage.$$('#nearbyLinkWrapper').click();
      await flushAsync();
      assertEquals(routes.NEARBY_SHARE, router.getCurrentRoute());
      assertTrue(router.getQueryParameters().has('onboarding'));
    });

    test(
        'Clicking nearby subpage before onboarding enters subpage',
        async () => {
          setNearbyShareDisallowedByPolicy(false);
          await flushAsync();

          const router = Router.getInstance();
          assertTrue(!!multidevicePage.$$('#nearbyLinkWrapper'));

          multidevicePage.$$('#nearbyLinkWrapper').click();
          await flushAsync();
          assertEquals(routes.NEARBY_SHARE, router.getCurrentRoute());
          assertFalse(router.getQueryParameters().has('onboarding'));
        });
  });
});
