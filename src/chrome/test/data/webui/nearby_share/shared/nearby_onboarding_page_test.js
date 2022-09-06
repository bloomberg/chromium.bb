// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://nearby/strings.m.js';
import 'chrome://nearby/shared/nearby_onboarding_page.js';

import {setContactManagerForTesting} from 'chrome://nearby/shared/nearby_contact_manager.js';
import {setNearbyShareSettingsForTesting} from 'chrome://nearby/shared/nearby_share_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {waitAfterNextRender} from '../../test_util.js';

import {FakeNearbyShareSettings} from './fake_nearby_share_settings.js';

suite('nearby-onboarding-page', function() {
  /** @type {!NearbyOnboardingPageElement} */
  let element;
  /** @type {!string} */
  const deviceName = 'Test\'s Device';
  /** @type {!FakeNearbyShareSettings} */
  let fakeSettings;

  setup(function() {
    fakeSettings = new FakeNearbyShareSettings();
    fakeSettings.setEnabled(true);
    setNearbyShareSettingsForTesting(fakeSettings);

    document.body.innerHTML = '';

    element = /** @type {!NearbyOnboardingPageElement} */ (
        document.createElement('nearby-onboarding-page'));
    element.settings = {
      enabled: false,
      fastInitiationNotificationState:
          nearbyShare.mojom.FastInitiationNotificationState.kEnabled,
      isFastInitiationHardwareSupported: true,
      deviceName: deviceName,
      dataUsage: nearbyShare.mojom.DataUsage.kOnline,
      visibility: nearbyShare.mojom.Visibility.kAllContacts,
      isOnboardingComplete: false,
      allowedContacts: [],
    };
    document.body.appendChild(element);
    element.fire('view-enter-start');
  });

  test('Renders onboarding page', async function() {
    assertEquals('NEARBY-ONBOARDING-PAGE', element.tagName);
    // Verify the device name is shown correctly.
    assertEquals(
        deviceName, element.shadowRoot.querySelector('#deviceName').value);
  });

  test('Device name is focused', async () => {
    const input = /** @type {!CrInputElement} */ (
        element.shadowRoot.querySelector('#deviceName'));
    await waitAfterNextRender(/** @type {!HTMLElement} */ (input));
    assertEquals(input, element.shadowRoot.activeElement);
  });

  test('validate device name preference', async () => {
    loadTimeData.overrideValues({
      'nearbyShareDeviceNameEmptyError': 'non-empty',
      'nearbyShareDeviceNameTooLongError': 'non-empty',
      'nearbyShareDeviceNameInvalidCharactersError': 'non-empty'
    });

    const input = /** @type {!CrInputElement} */ (
        element.shadowRoot.querySelector('#deviceName'));
    const pageTemplate =
        element.shadowRoot.querySelector('nearby-page-template');

    fakeSettings.setNextDeviceNameResult(
        nearbyShare.mojom.DeviceNameValidationResult.kErrorEmpty);
    input.fire('input');
    // Allow the validation promise to resolve.
    await waitAfterNextRender(/** @type {!HTMLElement} */ (input));
    assertTrue(input.invalid);
    assertTrue(pageTemplate.actionDisabled);

    fakeSettings.setNextDeviceNameResult(
        nearbyShare.mojom.DeviceNameValidationResult.kValid);
    input.fire('input');
    await waitAfterNextRender(/** @type {!HTMLElement} */ (input));
    assertFalse(input.invalid);
    assertFalse(pageTemplate.actionDisabled);
  });
});
