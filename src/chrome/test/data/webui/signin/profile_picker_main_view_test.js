// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ManageProfilesBrowserProxyImpl, NavigationMixin, Routes} from 'chrome://profile-picker/profile_picker.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertEquals, assertTrue} from '../chai_assert.js';
import {flushTasks, waitBeforeNextRender} from '../test_util.js';

import {TestManageProfilesBrowserProxy} from './test_manage_profiles_browser_proxy.js';


suite('ProfilePickerMainViewTest', function() {
  /** @type {!ProfilePickerMainViewElement} */
  let mainViewElement;

  /** @type {!TestManageProfilesBrowserProxy} */
  let browserProxy;

  let navigationElement;
  suiteSetup(function() {
    class NavigationElement extends NavigationMixin
    (PolymerElement) {
      static get is() {
        return 'navigation-element';
      }

      /** @override */
      ready() {
        super.ready();
        this.reset();
      }

      /**
       * @param {Routes} route
       * @param {string} step
       */
      onRouteChange(route, step) {
        this.changeCalled = true;
        this.route = route;
      }

      reset() {
        this.changeCalled = false;
        this.route = '';
      }
    }

    customElements.define(NavigationElement.is, NavigationElement);
  });

  function resetTest() {
    document.body.innerHTML = '';
    navigationElement = document.createElement('navigation-element');
    document.body.appendChild(navigationElement);
    mainViewElement = /** @type {!ProfilePickerMainViewElement} */ (
        document.createElement('profile-picker-main-view'));
    document.body.appendChild(mainViewElement);
    return waitBeforeNextRender(mainViewElement);
  }

  function resetPolicies() {
    // This is necessary as |loadTimeData| state leaks between tests.
    // Any load time data manipulated by the tests needs to be reset here.
    loadTimeData.overrideValues({
      isGuestModeEnabled: true,
      isProfileCreationAllowed: true,
      isAskOnStartupAllowed: true
    });
  }

  setup(function() {
    browserProxy = new TestManageProfilesBrowserProxy();
    ManageProfilesBrowserProxyImpl.setInstance(browserProxy);
    resetPolicies();
    return resetTest();
  });

  /**
   * @param {number} n Indicates the desired number of profiles.
   * @return {!Array<!ProfileState>} Array of profiles.
   */
  function generateProfilesList(n) {
    return Array(n)
        .fill(0)
        .map((x, i) => i % 2 === 0)
        .map((sync, i) => ({
               profilePath: `profilePath${i}`,
               localProfileName: `profile${i}`,
               isSyncing: sync,
               needsSignin: false,
               gaiaName: sync ? `User${i}` : '',
               userName: sync ? `User${i}@gmail.com` : '',
               isManaged: i % 4 === 0,
               avatarIcon: `AvatarUrl-${i}`,
             }));
  }

  /**
   * @param {!Array<!ProfileState>} expectedProfiles
   * @param {!Array<!ProfileCardElement>} Array of profiles.
   */
  async function verifyProfileCard(expectedProfiles, profiles) {
    assertEquals(expectedProfiles.length, profiles.length);
    for (let i = 0; i < expectedProfiles.length; i++) {
      assertTrue(!!profiles[i].shadowRoot.querySelector('profile-card-menu'));
      profiles[i].shadowRoot.querySelector('cr-button').click();
      await browserProxy.whenCalled('launchSelectedProfile');
      assertEquals(
          profiles[i].shadowRoot.querySelector('#forceSigninContainer').hidden,
          !expectedProfiles[i].needsSignin);

      const gaiaName = profiles[i].shadowRoot.querySelector('#gaiaName');
      assertEquals(gaiaName.hidden, expectedProfiles[i].needsSignin);
      assertEquals(gaiaName.innerText.trim(), expectedProfiles[i].gaiaName);

      assertEquals(
          profiles[i].shadowRoot.querySelector('#nameInput').value,
          expectedProfiles[i].localProfileName);
      assertEquals(
          profiles[i].shadowRoot.querySelector('#iconContainer').hidden,
          !expectedProfiles[i].isManaged);
      assertEquals(
          (profiles[i].shadowRoot.querySelector('.profile-avatar').src)
              .split('/')
              .pop(),
          expectedProfiles[i].avatarIcon);
    }
  }

  test('MainViewWithDefaultPolicies', async function() {
    assertTrue(navigationElement.changeCalled);
    assertEquals(navigationElement.route, Routes.MAIN);
    await browserProxy.whenCalled('initializeMainView');
    // Hidden while profiles list is not yet defined.
    assertTrue(mainViewElement.shadowRoot.querySelector('#wrapper').hidden);
    assertTrue(mainViewElement.shadowRoot.querySelector('cr-checkbox').hidden);
    const profiles = generateProfilesList(6);
    webUIListenerCallback('profiles-list-changed', [...profiles]);
    flushTasks();
    // Profiles list defined.
    assertTrue(!mainViewElement.shadowRoot.querySelector('#wrapper').hidden);
    assertTrue(!mainViewElement.shadowRoot.querySelector('cr-checkbox').hidden);
    assertTrue(mainViewElement.shadowRoot.querySelector('cr-checkbox').checked);
    // Verify profile card.
    await verifyProfileCard(
        profiles, mainViewElement.shadowRoot.querySelectorAll('profile-card'));
    // Browse as guest.
    assertTrue(
        !!mainViewElement.shadowRoot.querySelector('#browseAsGuestButton'));
    mainViewElement.shadowRoot.querySelector('#browseAsGuestButton').click();
    await browserProxy.whenCalled('launchGuestProfile');
    // Ask when chrome opens.
    mainViewElement.shadowRoot.querySelector('cr-checkbox').click();
    await browserProxy.whenCalled('askOnStartupChanged');
    assertTrue(
        !mainViewElement.shadowRoot.querySelector('cr-checkbox').checked);
    // Update profile data.
    profiles[1] = profiles[4];
    webUIListenerCallback('profiles-list-changed', [...profiles]);
    flushTasks();
    await verifyProfileCard(
        profiles, mainViewElement.shadowRoot.querySelectorAll('profile-card'));
    // Profiles update on remove.
    webUIListenerCallback('profile-removed', profiles[3].profilePath);
    profiles.splice(3, 1);
    flushTasks();
    await verifyProfileCard(
        profiles, mainViewElement.shadowRoot.querySelectorAll('profile-card'));
  });

  test('EditLocalProfileName', async function() {
    await browserProxy.whenCalled('initializeMainView');
    const profiles = generateProfilesList(1);
    webUIListenerCallback('profiles-list-changed', [...profiles]);
    flushTasks();
    const localProfileName =
        mainViewElement.shadowRoot.querySelector('profile-card')
            .shadowRoot.querySelector('#nameInput');
    assertEquals(localProfileName.value, profiles[0].localProfileName);

    // Set to valid profile name.
    localProfileName.value = 'Alice';
    localProfileName.dispatchEvent(
        new CustomEvent('change', {bubbles: true, composed: true}));
    const args = await browserProxy.whenCalled('setProfileName');
    assertEquals(args[0], profiles[0].profilePath);
    assertEquals(args[1], 'Alice');
    assertEquals(localProfileName.value, 'Alice');

    // Set to invalid profile name
    localProfileName.value = '';
    assertTrue(localProfileName.invalid);
  });

  test('GuestModeDisabled', async function() {
    loadTimeData.overrideValues({
      isGuestModeEnabled: false,
    });
    resetTest();
    assertEquals(
        mainViewElement.shadowRoot.querySelector('#browseAsGuestButton')
            .style.display,
        'none');
    await browserProxy.whenCalled('initializeMainView');
    webUIListenerCallback('profiles-list-changed', generateProfilesList(2));
    flushTasks();
    assertEquals(
        mainViewElement.shadowRoot.querySelector('#browseAsGuestButton')
            .style.display,
        'none');
  });

  test('ProfileCreationNotAllowed', async function() {
    loadTimeData.overrideValues({
      isProfileCreationAllowed: false,
    });
    resetTest();
    assertEquals(
        mainViewElement.shadowRoot.querySelector('#addProfile').style.display,
        'none');
    await browserProxy.whenCalled('initializeMainView');
    webUIListenerCallback('profiles-list-changed', generateProfilesList(2));
    flushTasks();
    navigationElement.reset();
    assertEquals(
        mainViewElement.shadowRoot.querySelector('#addProfile').style.display,
        'none');
    mainViewElement.shadowRoot.querySelector('#addProfile').click();
    flushTasks();
    assertTrue(!navigationElement.changeCalled);
  });

  test('AskOnStartupSingleToMultipleProfiles', async function() {
    await browserProxy.whenCalled('initializeMainView');
    // Hidden while profiles list is not yet defined.
    assertTrue(mainViewElement.shadowRoot.querySelector('#wrapper').hidden);
    assertTrue(mainViewElement.shadowRoot.querySelector('cr-checkbox').hidden);
    let profiles = generateProfilesList(1);
    webUIListenerCallback('profiles-list-changed', [...profiles]);
    flushTasks();
    await verifyProfileCard(
        profiles, mainViewElement.shadowRoot.querySelectorAll('profile-card'));
    // The checkbox 'Ask when chrome opens' should only be visible to
    // multi-profile users.
    assertTrue(mainViewElement.shadowRoot.querySelector('cr-checkbox').hidden);
    // Add a second profile.
    profiles = generateProfilesList(2);
    webUIListenerCallback('profiles-list-changed', [...profiles]);
    flushTasks();
    await verifyProfileCard(
        profiles, mainViewElement.shadowRoot.querySelectorAll('profile-card'));
    assertTrue(!mainViewElement.shadowRoot.querySelector('cr-checkbox').hidden);
    assertTrue(mainViewElement.shadowRoot.querySelector('cr-checkbox').checked);
    mainViewElement.shadowRoot.querySelector('cr-checkbox').click();
    await browserProxy.whenCalled('askOnStartupChanged');
    assertTrue(
        !mainViewElement.shadowRoot.querySelector('cr-checkbox').checked);
  });

  test('AskOnStartupMultipleToSingleProfile', async function() {
    await browserProxy.whenCalled('initializeMainView');
    // Hidden while profiles list is not yet defined.
    assertTrue(mainViewElement.shadowRoot.querySelector('#wrapper').hidden);
    assertTrue(mainViewElement.shadowRoot.querySelector('cr-checkbox').hidden);
    const profiles = generateProfilesList(2);
    webUIListenerCallback('profiles-list-changed', [...profiles]);
    flushTasks();
    await verifyProfileCard(
        profiles, mainViewElement.shadowRoot.querySelectorAll('profile-card'));
    assertTrue(!mainViewElement.shadowRoot.querySelector('cr-checkbox').hidden);
    // Remove profile.
    webUIListenerCallback('profile-removed', profiles[0].profilePath);
    flushTasks();
    await verifyProfileCard(
        [profiles[1]],
        mainViewElement.shadowRoot.querySelectorAll('profile-card'));
    assertTrue(mainViewElement.shadowRoot.querySelector('cr-checkbox').hidden);
  });

  test('AskOnStartupMulipleProfiles', async function() {
    // Disable AskOnStartup
    loadTimeData.overrideValues({isAskOnStartupAllowed: false});
    resetTest();

    await browserProxy.whenCalled('initializeMainView');
    // Hidden while profiles list is not yet defined.
    assertTrue(mainViewElement.shadowRoot.querySelector('#wrapper').hidden);
    assertTrue(mainViewElement.shadowRoot.querySelector('cr-checkbox').hidden);
    const profiles = generateProfilesList(2);
    webUIListenerCallback('profiles-list-changed', [...profiles]);
    flushTasks();
    await verifyProfileCard(
        profiles, mainViewElement.shadowRoot.querySelectorAll('profile-card'));

    // Checkbox hidden even if there are multiple profiles.
    assertTrue(mainViewElement.shadowRoot.querySelector('cr-checkbox').hidden);
  });

  test('ForceSigninIsEnabled', async function() {
    loadTimeData.overrideValues({isForceSigninEnabled: true});
    resetTest();

    await browserProxy.whenCalled('initializeMainView');
    const profiles = generateProfilesList(2);
    profiles[0].needsSignin = true;
    webUIListenerCallback('profiles-list-changed', [...profiles]);
    flushTasks();
    await verifyProfileCard(
        profiles, mainViewElement.shadowRoot.querySelectorAll('profile-card'));
  });
});
