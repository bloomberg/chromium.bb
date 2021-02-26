// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://profile-customization/profile_customization_app.js';

import {ProfileCustomizationBrowserProxyImpl} from 'chrome://profile-customization/profile_customization_browser_proxy.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

import {assertEquals, assertFalse, assertTrue} from '../chai_assert.js';
import {isChildVisible} from '../test_util.m.js';

import {TestProfileCustomizationBrowserProxy} from './test_profile_customization_browser_proxy.js';

suite('ProfileCustomizationTest', function() {
  /** @type {!ProfileCustomizationAppElement} */
  let app;

  /** @type {!TestProfileCustomizationBrowserProxy} */
  let browserProxy;

  /** @type {string} */
  const AVATAR_URL_1 = 'chrome://theme/IDR_PROFILE_AVATAR_1';
  /** @type {string} */
  const AVATAR_URL_2 = 'chrome://theme/IDR_PROFILE_AVATAR_2';

  setup(function() {
    loadTimeData.overrideValues({
      isManaged: false,
      profileName: 'TestName',
    });
    browserProxy = new TestProfileCustomizationBrowserProxy();
    browserProxy.setProfileInfo({
      textColor: 'rgb(255, 0, 0)',
      backgroundColor: 'rgb(0, 255, 0)',
      pictureUrl: AVATAR_URL_1,
    });
    ProfileCustomizationBrowserProxyImpl.instance_ = browserProxy;
    document.body.innerHTML = '';
    app = /** @type {!ProfileCustomizationAppElement} */ (
        document.createElement('profile-customization-app'));
    document.body.append(app);
    return browserProxy.whenCalled('initialized');
  });

  function checkImageUrl(elementId, expectedUrl) {
    assertTrue(isChildVisible(app, elementId));
    const img = app.$$(elementId);
    assertEquals(expectedUrl, img.src);
  }

  // Checks that clicking Done without interacting with the input does not
  // change the name.
  test('ClickDone', function() {
    assertTrue(isChildVisible(app, '#doneButton'));
    const doneButton = app.$$('#doneButton');
    assertFalse(doneButton.disabled);
    doneButton.click();
    return browserProxy.whenCalled('done').then(
        profileName => assertEquals('TestName', profileName));
  });

  // Checks that the name can be changed.
  test('ChangeName', function() {
    const nameInput = app.$$('#nameInput');
    // Check the default value for the input.
    assertEquals('TestName', nameInput.value);
    assertFalse(nameInput.invalid);

    // Invalid name (white space).
    nameInput.value = '   ';
    assertTrue(nameInput.invalid);

    // The button is disabled.
    assertTrue(isChildVisible(app, '#doneButton'));
    const doneButton = app.$$('#doneButton');
    assertTrue(doneButton.disabled);

    // Empty name.
    nameInput.value = '';
    assertTrue(nameInput.invalid);
    assertTrue(doneButton.disabled);

    // Valid name.
    nameInput.value = 'Bob';
    assertFalse(nameInput.invalid);

    // Click done, and check that the new name is sent.
    assertTrue(isChildVisible(app, '#doneButton'));
    assertFalse(doneButton.disabled);
    doneButton.click();
    return browserProxy.whenCalled('done').then(
        profileName => assertEquals('Bob', profileName));
  });

  test('ProfileInfo', function() {
    const header = app.$$('#header');
    // Check initial info.
    assertEquals('rgb(255, 0, 0)', getComputedStyle(header).color);
    assertEquals('rgb(0, 255, 0)', getComputedStyle(header).backgroundColor);
    checkImageUrl('#avatar', AVATAR_URL_1);
    // Update the info.
    const color1 = 'rgb(1, 2, 3)';
    const color2 = 'rgb(4, 5, 6)';
    webUIListenerCallback(
        'on-profile-info-changed',
        {textColor: color1, backgroundColor: color2, pictureUrl: AVATAR_URL_2});
    assertEquals(color1, getComputedStyle(header).color);
    assertEquals(color2, getComputedStyle(header).backgroundColor);
    checkImageUrl('#avatar', AVATAR_URL_2);
  });

  test('ThemeSelector', function() {
    assertTrue(!!app.$$('#themeSelector'));
  });
});
