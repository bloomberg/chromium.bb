// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EnterpriseProfileWelcomeAppElement} from 'chrome://enterprise-profile-welcome/enterprise_profile_welcome_app.js';

import {EnterpriseProfileWelcomeBrowserProxyImpl} from 'chrome://enterprise-profile-welcome/enterprise_profile_welcome_browser_proxy.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';

import {assertEquals, assertFalse, assertTrue} from '../chai_assert.js';
import {isChildVisible, waitAfterNextRender} from '../test_util.m.js';

import {TestEnterpriseProfileWelcomeBrowserProxy} from './test_enterprise_profile_welcome_browser_proxy.js';

suite('EnterpriseProfileWelcomeTest', function() {
  /** @type {!EnterpriseProfileWelcomeAppElement} */
  let app;

  /** @type {!TestEnterpriseProfileWelcomeBrowserProxy} */
  let browserProxy;

  /** @type {string} */
  const AVATAR_URL_1 = 'chrome://theme/IDR_PROFILE_AVATAR_1';
  /** @type {string} */
  const AVATAR_URL_2 = 'chrome://theme/IDR_PROFILE_AVATAR_2';

  setup(async function() {
    browserProxy = new TestEnterpriseProfileWelcomeBrowserProxy({
      backgroundColor: 'rgb(255, 0, 0)',
      pictureUrl: AVATAR_URL_1,
      showEnterpriseBadge: false,
      enterpriseTitle: 'enterprise_title',
      enterpriseInfo: 'enterprise_info',
      proceedLabel: 'proceed_label',
    });
    EnterpriseProfileWelcomeBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = '';
    app = /** @type {!EnterpriseProfileWelcomeAppElement} */ (
        document.createElement('enterprise-profile-welcome-app'));
    document.body.appendChild(app);
    await waitAfterNextRender(app);
    return browserProxy.whenCalled('initialized');
  });

  /**
   * Checks that the expected image url is displayed.
   * @param {string} expectedUrl
   */
  function checkImageUrl(expectedUrl) {
    assertTrue(isChildVisible(app, '#avatar'));
    const img = app.shadowRoot.querySelector('#avatar');
    assertEquals(expectedUrl, img.src);
  }

  /**
   * Checks that the expected header color is displayed.
   * @param {string} expectedColor
   */
  function checkHeaderColor(expectedColor) {
    assertTrue(isChildVisible(app, '#headerContainer'));
    const headerElement = app.shadowRoot.querySelector('#headerContainer');
    assertEquals(
        expectedColor, getComputedStyle(headerElement).backgroundColor);
  }

  test('proceed', async function() {
    assertTrue(isChildVisible(app, '#proceedButton'));
    app.shadowRoot.querySelector('#proceedButton').click();
    await browserProxy.whenCalled('proceed');
  });

  test('cancel', async function() {
    assertTrue(isChildVisible(app, '#cancelButton'));
    app.shadowRoot.querySelector('#cancelButton').click();
    await browserProxy.whenCalled('cancel');
  });

  test('onProfileInfoChanged', function() {
    // Helper to test all the text values in the UI.
    function checkTextValues(
        expectedEnterpriseTitle, expectedEnterpriseInfo, expectedProceedLabel) {
      assertTrue(isChildVisible(app, '#enterpriseTitle'));
      const enterpriseTitleElement =
          app.shadowRoot.querySelector('#enterpriseTitle');
      assertEquals(
          expectedEnterpriseTitle, enterpriseTitleElement.textContent.trim());
      assertTrue(isChildVisible(app, '#enterpriseInfo'));
      const enterpriseInfoElement =
          app.shadowRoot.querySelector('#enterpriseInfo');
      assertEquals(
          expectedEnterpriseInfo, enterpriseInfoElement.textContent.trim());
      assertTrue(isChildVisible(app, '#proceedButton'));
      const proceedButton = app.shadowRoot.querySelector('#proceedButton');
      assertEquals(expectedProceedLabel, proceedButton.textContent.trim());
    }

    // Initial values.
    checkTextValues('enterprise_title', 'enterprise_info', 'proceed_label');
    checkImageUrl(AVATAR_URL_1);
    assertFalse(isChildVisible(app, '.work-badge'));
    checkHeaderColor('rgb(255, 0, 0)');

    // Update the values.
    webUIListenerCallback('on-profile-info-changed', {
      backgroundColor: 'rgb(0, 255, 0)',
      pictureUrl: AVATAR_URL_2,
      showEnterpriseBadge: true,
      enterpriseTitle: 'new_enterprise_title',
      enterpriseInfo: 'new_enterprise_info',
      proceedLabel: 'new_proceed_label',
    });

    checkTextValues(
        'new_enterprise_title', 'new_enterprise_info', 'new_proceed_label');
    checkImageUrl(AVATAR_URL_2);
    assertTrue(isChildVisible(app, '.work-badge'));
    checkHeaderColor('rgb(0, 255, 0)');
  });
});
