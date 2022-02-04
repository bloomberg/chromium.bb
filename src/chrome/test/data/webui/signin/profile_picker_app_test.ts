// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://profile-picker/profile_picker.js';
// <if expr="lacros">
import 'chrome://profile-picker/lazy_load.js';
// </if>

// <if expr="lacros">
import {AvailableAccount} from 'chrome://profile-picker/profile_picker.js';
// </if>

import {ensureLazyLoaded, ManageProfilesBrowserProxyImpl, navigateTo, ProfilePickerAppElement, Routes} from 'chrome://profile-picker/profile_picker.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitBeforeNextRender, whenCheck} from 'chrome://webui-test/test_util.js';

import {TestManageProfilesBrowserProxy} from './test_manage_profiles_browser_proxy.js';

suite('ProfilePickerAppTest', function() {
  let testElement: ProfilePickerAppElement;
  let browserProxy: TestManageProfilesBrowserProxy;

  function resetTestElement(route: Routes) {
    document.body.innerHTML = '';
    navigateTo(route);
    testElement = document.createElement('profile-picker-app');
    document.body.appendChild(testElement);
    return waitBeforeNextRender(testElement);
  }

  setup(function() {
    browserProxy = new TestManageProfilesBrowserProxy();
    ManageProfilesBrowserProxyImpl.setInstance(browserProxy);

    return resetTestElement(Routes.MAIN);
  });

  /**
   * @return Promise that resolves when initialization is complete
   *     and the lazy loaded module has been loaded.
   */
  async function waitForProfileCreationLoad(): Promise<void> {
    await Promise.all([
      browserProxy.whenCalled('getNewProfileSuggestedThemeInfo'),
      ensureLazyLoaded(),
    ]);
    browserProxy.reset();
  }

  function verifyProfileCreationViewStyle(element: HTMLElement) {
    assertEquals(
        getComputedStyle(element.shadowRoot!.querySelector('#headerContainer')!)
            .getPropertyValue('--theme-frame-color')
            .trim(),
        browserProxy.profileThemeInfo.themeFrameColor);
    assertEquals(
        getComputedStyle(element.shadowRoot!.querySelector('#headerContainer')!)
            .getPropertyValue('--theme-text-color')
            .trim(),
        browserProxy.profileThemeInfo.themeFrameTextColor);
    assertEquals(
        getComputedStyle(element.shadowRoot!.querySelector('#headerContainer')!)
            .backgroundColor,
        browserProxy.profileThemeInfo.themeFrameColor);
    assertEquals(
        getComputedStyle(element.shadowRoot!.querySelector('#backButton')!)
            .getPropertyValue('--cr-icon-button-fill-color')
            .trim(),
        browserProxy.profileThemeInfo.themeFrameTextColor);
  }

  test('ProfilePickerMainView', async function() {
    assertEquals(
        testElement.shadowRoot!.querySelectorAll('[slot=view]').length, 1);
    const mainView =
        testElement.shadowRoot!.querySelector('profile-picker-main-view')!;
    await whenCheck(mainView, () => mainView.classList.contains('active'));
    await browserProxy.whenCalled('initializeMainView');
    const wrapper =
        mainView.shadowRoot!.querySelector<HTMLElement>('#wrapper')!;
    assertTrue(wrapper.hidden);

    webUIListenerCallback(
        'profiles-list-changed', [browserProxy.profileSample]);
    flushTasks();
    assertEquals(wrapper.querySelectorAll('profile-card').length, 1);
    mainView.$.addProfile.click();
    await waitForProfileCreationLoad();
    assertEquals(
        testElement.shadowRoot!.querySelectorAll('[slot=view]').length, 2);
    const choice = testElement.shadowRoot!.querySelector('profile-type-choice');
    assertTrue(!!choice);
    await whenCheck(choice!, () => choice!.classList.contains('active'));
    verifyProfileCreationViewStyle(choice!);
  });

  // <if expr="lacros">
  test('SignInPromoSignInWithAvailableAccountLacros', async function() {
    await resetTestElement(Routes.NEW_PROFILE);
    await waitForProfileCreationLoad();
    const choice =
        testElement.shadowRoot!.querySelector('profile-type-choice')!;
    assertTrue(!!choice);
    // Add available account to trigger the account selection screen.
    const availableAccount: AvailableAccount = {
      gaiaId: 'available-id',
      name: 'Account Name',
      email: 'email@gmail.com',
      accountImageUrl: 'account-image-url',
    };
    webUIListenerCallback('available-accounts-changed', [availableAccount]);
    flushTasks();
    choice!.$.signInButton.click();
    // Start Lacros signin flow.
    await waitBeforeNextRender(testElement);
    const accountSelectionLacros =
        testElement.shadowRoot!.querySelector('account-selection-lacros');
    assertTrue(!!accountSelectionLacros);
    // Test the back button.
    const backButton =
        accountSelectionLacros!.shadowRoot!.querySelector<HTMLElement>(
            '#backButton');
    assertTrue(!!backButton);
    backButton!.click();
    await whenCheck(choice!, () => choice!.classList.contains('active'));
  });

  test('SignInPromoSignInWithoutAccountLacros', async function() {
    await resetTestElement(Routes.NEW_PROFILE);
    await waitForProfileCreationLoad();
    const choice = testElement.shadowRoot!.querySelector('profile-type-choice');
    assertTrue(!!choice);
    // No available account.
    webUIListenerCallback('available-accounts-changed', []);
    flushTasks();
    choice!.$.signInButton.click();
    return browserProxy.whenCalled('loadSignInProfileCreationFlow');
  });
  // </if>

  test('SignInPromoSignIn', async function() {
    await resetTestElement(Routes.NEW_PROFILE);
    await waitForProfileCreationLoad();
    const choice = testElement.shadowRoot!.querySelector('profile-type-choice');
    assertTrue(!!choice);
    choice!.$.signInButton.click();
    assertTrue(choice!.$.signInButton.disabled);
    assertTrue(choice!.$.notNowButton.disabled);
    assertTrue(choice!.$.backButton.disabled);
    return browserProxy.whenCalled('loadSignInProfileCreationFlow');
  });

  test('ThemeColorConsistentInProfileCreationViews', async function() {
    await resetTestElement(Routes.NEW_PROFILE);
    await waitForProfileCreationLoad();
    const choice = testElement.shadowRoot!.querySelector('profile-type-choice');
    assertTrue(!!choice);
    await whenCheck(choice!, () => choice!.classList.contains('active'));
    verifyProfileCreationViewStyle(choice!);
    choice!.$.notNowButton.click();
    await waitBeforeNextRender(testElement);
    const customization =
        testElement.shadowRoot!.querySelector('local-profile-customization');
    assertTrue(!!customization);
    await whenCheck(
        customization!, () => customization!.classList.contains('active'));
    verifyProfileCreationViewStyle(customization!);

    // Test color changes from the local profile customization is reflected in
    // the profile type choice.
    browserProxy.resetResolver('getProfileThemeInfo');
    const colorPicker =
        customization!.shadowRoot!.querySelector('cr-customize-themes');
    assertTrue(!!colorPicker);
    assertTrue(!!colorPicker!.selectedTheme);
    browserProxy.setProfileThemeInfo({
      color: -3413569,
      colorId: 7,
      themeFrameColor: 'rgb(203, 233, 191)',
      themeFrameTextColor: 'rgb(32, 33, 36)',
      themeGenericAvatar: 'AvatarUrl-7',
      themeShapeColor: 'rgb(255, 255, 255)'
    });
    // Select different color.
    colorPicker!.selectedTheme = {
      type: 2,
      info: {
        chromeThemeId: browserProxy.profileThemeInfo.colorId,
        autogeneratedThemeColors: undefined,
        thirdPartyThemeInfo: undefined,
      },
      isForced: false,
    };
    await browserProxy.whenCalled('getProfileThemeInfo');
    verifyProfileCreationViewStyle(customization!);
    customization!.$.backButton.click();
    await whenCheck(choice!, () => choice!.classList.contains('active'));
    verifyProfileCreationViewStyle(choice!);
  });

  test('ProfileCreationNotAllowed', async function() {
    loadTimeData.overrideValues({
      isProfileCreationAllowed: false,
    });
    await resetTestElement(Routes.NEW_PROFILE);
    assertEquals(
        testElement.shadowRoot!.querySelectorAll('[slot=view]').length, 1);
    const mainView =
        testElement.shadowRoot!.querySelector('profile-picker-main-view')!;
    await whenCheck(mainView, () => mainView.classList.contains('active'));
  });

  test('ForceSignInEnabled', async function() {
    loadTimeData.overrideValues({
      isProfileCreationAllowed: true,
      isForceSigninEnabled: true,
    });
    await resetTestElement(Routes.NEW_PROFILE);
    assertEquals(
        testElement.shadowRoot!.querySelectorAll('[slot=view]').length, 1);
    const mainView =
        testElement.shadowRoot!.querySelector('profile-picker-main-view')!;
    await whenCheck(mainView, () => mainView.classList.contains('active'));
    await browserProxy.whenCalled('loadSignInProfileCreationFlow');
  });
});
