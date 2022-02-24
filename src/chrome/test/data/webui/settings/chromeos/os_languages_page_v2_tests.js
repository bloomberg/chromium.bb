// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {LanguagesBrowserProxyImpl, LanguagesMetricsProxyImpl, LanguagesPageInteraction, LifetimeBrowserProxyImpl} from 'chrome://os-settings/chromeos/lazy_load.js';
// #import {CrSettingsPrefs, Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {assert} from 'chrome://resources/js/assert.m.js';
// #import {keyDownOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {getFakeLanguagePrefs} from './fake_language_settings_private.js'
// #import {FakeSettingsPrivate} from '../fake_settings_private.js';
// #import {TestLanguagesBrowserProxy} from './test_os_languages_browser_proxy.m.js';
// #import {TestLanguagesMetricsProxy} from './test_os_languages_metrics_proxy.m.js';
// #import {TestLifetimeBrowserProxy} from './test_os_lifetime_browser_proxy.m.js';
// #import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
// #import {fakeDataBind} from '../../test_util.js';
// #import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
// #import {waitAfterNextRender} from 'chrome://test/test_util.js';
// clang-format on

suite('languages page', () => {
  /** @type {!LanguageHelper} */
  let languageHelper;
  /** @type {!SettingsLanguagesPageElement} */
  let languagesPage;
  /** @type {!HTMLElement} */
  let languagesList;
  /** @type {!CrActionMenuElement} */
  let actionMenu;
  /** @type {!settings.LanguagesBrowserProxy} */
  let browserProxy;
  /** @type {!settings.TestLifetimeBrowserProxy} */
  let lifetimeProxy;
  /** @type {!settings.LanguagesMetricsProxy} */
  let metricsProxy;

  // Initial value of enabled languages pref used in tests.
  const initialLanguages = 'en-US,sw';

  suiteSetup(() => {
    CrSettingsPrefs.deferInitialization = true;
  });

  setup(async () => {
    document.body.innerHTML = '';

    const settingsPrefs = document.createElement('settings-prefs');
    const settingsPrivate =
        new settings.FakeSettingsPrivate(settings.getFakeLanguagePrefs());
    settingsPrefs.initialize(settingsPrivate);
    document.body.appendChild(settingsPrefs);
    await CrSettingsPrefs.initialized;
    // Sets up test browser proxy.
    browserProxy = new settings.TestLanguagesBrowserProxy();
    settings.LanguagesBrowserProxyImpl.setInstance(browserProxy);

    lifetimeProxy = new settings.TestLifetimeBrowserProxy();
    settings.LifetimeBrowserProxyImpl.setInstance(lifetimeProxy);

    // Sets up test metrics proxy.
    metricsProxy = new settings.TestLanguagesMetricsProxy();
    settings.LanguagesMetricsProxyImpl.instance_ = metricsProxy;

    // Sets up fake languageSettingsPrivate API.
    const languageSettingsPrivate = browserProxy.getLanguageSettingsPrivate();
    languageSettingsPrivate.setSettingsPrefs(settingsPrefs);

    // Instantiates the data model with data bindings for prefs.
    const settingsLanguages = document.createElement('settings-languages');
    settingsLanguages.prefs = settingsPrefs.prefs;
    test_util.fakeDataBind(settingsPrefs, settingsLanguages, 'prefs');
    document.body.appendChild(settingsLanguages);

    // Creates page with data bindings for prefs and data model.
    languagesPage = document.createElement('os-settings-languages-page-v2');
    languagesPage.prefs = settingsPrefs.prefs;
    test_util.fakeDataBind(settingsPrefs, languagesPage, 'prefs');
    languagesPage.languages = settingsLanguages.languages;
    test_util.fakeDataBind(settingsLanguages, languagesPage, 'languages');
    languagesPage.languageHelper = settingsLanguages.languageHelper;
    test_util.fakeDataBind(settingsLanguages, languagesPage, 'language-helper');
    document.body.appendChild(languagesPage);

    languagesList = languagesPage.$.languagesList;
    actionMenu = languagesPage.$.menu.get();

    languageHelper = languagesPage.languageHelper;
    await languageHelper.whenReady();
  });

  teardown(function() {
    settings.Router.getInstance().resetRouteForTesting();
  });

  suite('language menu', () => {
    /*
     * Finds, asserts and returns the menu item for the given i18n key.
     * @param {string} i18nKey Name of the i18n string for the item's text.
     * @return {!HTMLElement} Menu item.
     */
    function getMenuItem(i18nKey) {
      const i18nString = assert(loadTimeData.getString(i18nKey));
      const menuItems = actionMenu.querySelectorAll('.dropdown-item');
      const menuItem = Array.from(menuItems).find(
          item => item.textContent.trim() === i18nString);
      return assert(menuItem, `Menu item "${i18nKey}" not found`);
    }

    /*
     * Checks the visibility of each expected menu item button.
     * param {!Object<boolean>} Dictionary from i18n keys to expected
     *     visibility of those menu items.
     */
    function assertMenuItemButtonsVisible(buttonVisibility) {
      assertTrue(actionMenu.open);
      for (const buttonKey of Object.keys(buttonVisibility)) {
        const buttonItem = getMenuItem(buttonKey);
        assertEquals(
            !buttonVisibility[buttonKey], buttonItem.hidden,
            `Menu item "${buttonKey}" hidden`);
      }
    }

    test('removes language when starting with 3 languages', () => {
      // Enables a language which we can then disable.
      languageHelper.enableLanguage('no');

      // Populates the dom-repeat.
      Polymer.dom.flush();

      // Finds the new language item.
      const items = languagesList.querySelectorAll('.list-item');
      const domRepeat = assert(languagesList.querySelector('dom-repeat'));
      const item = Array.from(items).find(function(el) {
        return domRepeat.itemForElement(el) &&
            domRepeat.itemForElement(el).language.code === 'no';
      });

      // Opens the menu and selects Remove.
      item.querySelector('cr-icon-button').click();

      assertTrue(actionMenu.open);
      const removeMenuItem = getMenuItem('removeLanguage');
      assertFalse(removeMenuItem.disabled);
      assertFalse(removeMenuItem.hidden);
      removeMenuItem.click();
      assertFalse(actionMenu.open);

      assertEquals(
          initialLanguages,
          languageHelper.getPref('intl.accept_languages').value);
    });

    test('removes language when starting with 2 languages', () => {
      const items = languagesList.querySelectorAll('.list-item');
      const domRepeat = assert(languagesList.querySelector('dom-repeat'));
      const item = Array.from(items).find(function(el) {
        return domRepeat.itemForElement(el) &&
            domRepeat.itemForElement(el).language.code === 'sw';
      });

      // Opens the menu and selects Remove.
      item.querySelector('cr-icon-button').click();

      assertTrue(actionMenu.open);
      const removeMenuItem = getMenuItem('removeLanguage');
      assertFalse(removeMenuItem.disabled);
      assertFalse(removeMenuItem.hidden);
      removeMenuItem.click();
      assertFalse(actionMenu.open);

      assertEquals(
          'en-US', languageHelper.getPref('intl.accept_languages').value);
    });

    test('the only translate blocked language is not removable', () => {
      //'en-US' is preconfigured to be the only translate blocked language.
      assertDeepEquals(
          ['en-US'], languageHelper.prefs.translate_blocked_languages.value);
      const items = languagesList.querySelectorAll('.list-item');
      const domRepeat = assert(languagesList.querySelector('dom-repeat'));
      const item = Array.from(items).find(function(el) {
        return domRepeat.itemForElement(el) &&
            domRepeat.itemForElement(el).language.code === 'en-US';
      });

      // Opens the menu and selects Remove.
      item.querySelector('cr-icon-button').click();

      assertTrue(actionMenu.open);
      const removeMenuItem = getMenuItem('removeLanguage');
      assertTrue(removeMenuItem.disabled);
      assertFalse(removeMenuItem.hidden);
    });

    test('device language is removable', () => {
      // 'en-US' is the preconfigured UI language.
      assertEquals('en-US', languageHelper.languages.prospectiveUILanguage);
      // Add 'sw' to translate_blocked_languages.
      languageHelper.setPrefValue(
          'translate_blocked_languages', ['en-US', 'sw']);
      Polymer.dom.flush();

      const items = languagesList.querySelectorAll('.list-item');
      const domRepeat = assert(languagesList.querySelector('dom-repeat'));
      const item = Array.from(items).find(function(el) {
        return domRepeat.itemForElement(el) &&
            domRepeat.itemForElement(el).language.code === 'en-US';
      });

      // Opens the menu and selects Remove.
      item.querySelector('cr-icon-button').click();

      assertTrue(actionMenu.open);
      const removeMenuItem = getMenuItem('removeLanguage');
      assertFalse(removeMenuItem.disabled);
      assertFalse(removeMenuItem.hidden);
      removeMenuItem.click();
      assertFalse(actionMenu.open);

      assertEquals('sw', languageHelper.getPref('intl.accept_languages').value);
    });

    test('single preferred language is not removable', () => {
      languageHelper.setPrefValue('intl.accept_languages', 'sw');
      languageHelper.setPrefValue(
          'settings.language.preferred_languages', 'sw');
      Polymer.dom.flush();
      const items = languagesList.querySelectorAll('.list-item');
      const domRepeat = assert(languagesList.querySelector('dom-repeat'));
      const item = Array.from(items).find(function(el) {
        return domRepeat.itemForElement(el) &&
            domRepeat.itemForElement(el).language.code === 'sw';
      });

      // Opens the menu and selects Remove.
      item.querySelector('cr-icon-button').click();

      assertTrue(actionMenu.open);
      const removeMenuItem = getMenuItem('removeLanguage');
      assertTrue(removeMenuItem.disabled);
      assertFalse(removeMenuItem.hidden);
    });

    test('removing a language does not remove related input methods', () => {
      const sw = '_comp_ime_abcdefghijklmnopqrstuvwxyzabcdefxkb:sw:sw';
      const swUS = 'ime_abcdefghijklmnopqrstuvwxyzabcdefxkb:us:sw';
      languageHelper.addInputMethod(sw);
      languageHelper.addInputMethod(swUS);
      assertEquals(4, languageHelper.languages.inputMethods.enabled.length);

      // Disable Swahili. The Swahili-only keyboard should not be removed.
      languageHelper.disableLanguage('sw');
      assertEquals(4, languageHelper.languages.inputMethods.enabled.length);
    });

    test('has move up/down buttons', () => {
      // Adds several languages.
      for (const language of ['en-CA', 'en-US', 'tk', 'no']) {
        languageHelper.enableLanguage(language);
      }

      Polymer.dom.flush();

      const menuButtons = languagesList.querySelectorAll(
          '.list-item cr-icon-button.icon-more-vert');

      // First language should not have "Move up" or "Move to top".
      menuButtons[0].click();
      assertMenuItemButtonsVisible({
        moveToTop: false,
        moveUp: false,
        moveDown: true,
      });
      actionMenu.close();

      // Second language should not have "Move up".
      menuButtons[1].click();
      assertMenuItemButtonsVisible({
        moveToTop: true,
        moveUp: false,
        moveDown: true,
      });
      actionMenu.close();

      // Middle languages should have all buttons.
      menuButtons[2].click();
      assertMenuItemButtonsVisible({
        moveToTop: true,
        moveUp: true,
        moveDown: true,
      });
      actionMenu.close();

      // Last language should not have "Move down".
      menuButtons[menuButtons.length - 1].click();
      assertMenuItemButtonsVisible({
        moveToTop: true,
        moveUp: true,
        moveDown: false,
      });
      actionMenu.close();
    });

    test('test translate target language is labelled', () => {
      const targetLanguageCode = languageHelper.languages.translateTarget;
      assertTrue(!!targetLanguageCode);

      // Add 'en' to have more than one translate-target language.
      languageHelper.enableLanguage('en');
      const isTranslateTarget = languageState =>
          languageHelper.convertLanguageCodeForTranslate(
              languageState.language.code) === targetLanguageCode;
      const translateTargets =
          languageHelper.languages.enabled.filter(isTranslateTarget);
      assertTrue(translateTargets.length > 1);
      // Ensure there is at least one non-translate-target language.
      assertTrue(
          translateTargets.length < languageHelper.languages.enabled.length);

      const listItems = languagesList.querySelectorAll('.list-item');
      const domRepeat = languagesList.querySelector('dom-repeat');
      assertTrue(!!domRepeat);

      let translateTargetLabel;
      let item;
      let num_visibles = 0;
      Array.from(listItems).forEach(el => {
        item = domRepeat.itemForElement(el);
        if (item) {
          translateTargetLabel = el.querySelector('.target-info');
          assertTrue(!!translateTargetLabel);
          if (getComputedStyle(translateTargetLabel).display !== 'none') {
            num_visibles++;
            assertEquals(
                targetLanguageCode,
                languageHelper.convertLanguageCodeForTranslate(
                    item.language.code));
          }
        }
        assertEquals(
            1, num_visibles,
            'Not exactly one target info label (' + num_visibles + ').');
      });
    });

    test('toggle translate checkbox for a language', async () => {
      // Open options for 'sw'.
      const languageOptionsDropdownTrigger =
          languagesList.querySelectorAll('cr-icon-button')[1];
      assertTrue(!!languageOptionsDropdownTrigger);
      languageOptionsDropdownTrigger.click();
      assertTrue(actionMenu.open);

      // 'sw' supports translate to the target language ('en').
      const translateOption = getMenuItem('offerToTranslateThisLanguage');
      assertFalse(translateOption.disabled);
      assertTrue(translateOption.checked);

      // Toggle the translate option.
      translateOption.click();
      assertFalse(
          await metricsProxy.whenCalled('recordTranslateCheckboxChanged'));
      assertDeepEquals(
          ['en-US', 'sw'],
          languageHelper.prefs.translate_blocked_languages.value);

      // Menu should stay open briefly.
      assertTrue(actionMenu.open);

      // Menu closes after delay
      const kMenuCloseDelay = 100;
      await new Promise(r => setTimeout(r, kMenuCloseDelay + 1));
      assertFalse(actionMenu.open);
    });

    test('translate checkbox disabled for translate blocked language', () => {
      // Open options for 'en-US'.
      const languageOptionsDropdownTrigger =
          languagesList.querySelectorAll('cr-icon-button')[0];
      assertTrue(!!languageOptionsDropdownTrigger);
      languageOptionsDropdownTrigger.click();
      assertTrue(actionMenu.open);

      // 'en-US' does not support checkbox.
      const translateOption = getMenuItem('offerToTranslateThisLanguage');
      assertTrue(translateOption.disabled);
    });

    test('disable translate hides language-specific option', () => {
      // Disables translate.
      languageHelper.setPrefValue('translate.enabled', false);

      // Open options for 'sw'.
      const languageOptionsDropdownTrigger =
          languagesList.querySelectorAll('cr-icon-button')[1];
      assertTrue(!!languageOptionsDropdownTrigger);
      languageOptionsDropdownTrigger.click();
      assertTrue(actionMenu.open);

      // The language-specific translation option should be hidden.
      const translateOption = actionMenu.querySelector('#offerTranslations');
      assertTrue(!!translateOption);
      assertTrue(translateOption.hidden);
    });

    test('Deep link to add language', async () => {
      const params = new URLSearchParams;
      params.append('settingId', '1200');
      settings.Router.getInstance().navigateTo(
          settings.routes.OS_LANGUAGES_LANGUAGES, params);

      Polymer.dom.flush();

      const deepLinkElement = languagesPage.$$('#addLanguages');
      await test_util.waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          'Add language button should be focused for settingId=1200.');
    });
  });

  suite('change device language dialog', () => {
    let dialog;
    let dialogItems;
    let cancelButton;
    let actionButton;

    /**
     * Returns the list items in the dialog.
     * @return {!Array<!Element>}
     */
    function getListItems() {
      // If an element (the <iron-list> in this case) is hidden in Polymer,
      // Polymer will intelligently not update the DOM of the hidden element
      // to prevent DOM updates that the user can't see. However, this means
      // that when the <iron-list> is hidden (due to no results), the list
      // items still exist in the DOM.
      // This function should return the *visible* items that the user can
      // select, so if the <iron-list> is hidden we should return an empty
      // list instead.
      const dialogEl = dialog.$.dialog;
      if (dialogEl.querySelector('iron-list').hidden) {
        return [];
      }
      return [...dialogEl.querySelectorAll('.list-item:not([hidden])')];
    }

    setup(() => {
      assertFalse(
          !!languagesPage.$$('os-settings-change-device-language-dialog'));
      languagesPage.$$('#changeDeviceLanguage').click();
      Polymer.dom.flush();

      dialog = languagesPage.$$('os-settings-change-device-language-dialog');
      assertTrue(!!dialog);

      actionButton = dialog.$$('.action-button');
      assertTrue(!!actionButton);
      cancelButton = dialog.$$('.cancel-button');
      assertTrue(!!cancelButton);

      // The fixed-height dialog's iron-list should stamp far fewer than
      // 50 items.
      dialogItems =
          dialog.$.dialog.querySelectorAll('.list-item:not([hidden])');
      assertGT(dialogItems.length, 1);
      assertLT(dialogItems.length, 50);

      // No language has been selected, so the action button is disabled.
      assertTrue(actionButton.disabled);
      assertFalse(cancelButton.disabled);
    });

    test('has action button working correctly', () => {
      // selecting a language enables action button
      dialogItems[0].click();
      assertFalse(actionButton.disabled);

      // selecting the same language again disables action button
      dialogItems[0].click();
      assertTrue(actionButton.disabled);
    });

    test('setting device language restarts device', async () => {
      // selects a language
      dialogItems[0].click();  // en-CA
      assertFalse(actionButton.disabled);

      actionButton.click();
      assertEquals(
          'en-CA', await browserProxy.whenCalled('setProspectiveUILanguage'));
      assertEquals(
          settings.LanguagesPageInteraction.RESTART,
          await metricsProxy.whenCalled('recordInteraction'));
      await lifetimeProxy.whenCalled('signOutAndRestart');
    });

    test(
        'setting device language adds it to front of enabled language if not present',
        async () => {
          languageHelper.setPrefValue('intl.accept_languages', 'en-US,sw');
          languageHelper.setPrefValue(
              'settings.language.preferred_languages', 'en-US,sw');
          // selects a language
          dialogItems[0].click();  // en-CA
          assertFalse(actionButton.disabled);

          actionButton.click();
          assertEquals(
              'en-CA',
              await browserProxy.whenCalled('setProspectiveUILanguage'));
          assertTrue(languageHelper.getPref('intl.accept_languages')
                         .value.startsWith('en-CA'));
        });

    test(
        'setting device language does not move already enabled language to front',
        async () => {
          languageHelper.setPrefValue(
              'intl.accept_languages', 'en-US,sw,en-CA');
          languageHelper.setPrefValue(
              'settings.language.preferred_languages', 'en-US,sw,en-CA');
          Polymer.dom.flush();

          // selects a language
          dialogItems[0].click();  // en-CA
          assertFalse(actionButton.disabled);

          actionButton.click();
          assertEquals(
              'en-CA',
              await browserProxy.whenCalled('setProspectiveUILanguage'));
          assertFalse(languageHelper.getPref('intl.accept_languages')
                          .value.startsWith('en-CA'));
        });

    // Test that searching languages works whether the displayed or native
    // language name is queried.
    test('searches languages', function() {
      const searchInput = dialog.$$('cr-search-field');

      // Expecting a few languages to be displayed when no query exists.
      assertGE(getListItems().length, 1);

      // Issue query that matches the |displayedName| in lowercase.
      searchInput.setValue('greek');
      Polymer.dom.flush();
      assertEquals(1, getListItems().length);
      assertTrue(getListItems()[0].textContent.includes('Greek'));

      // Issue query that matches the |nativeDisplayedName|.
      searchInput.setValue('Ελληνικά');
      Polymer.dom.flush();
      assertEquals(1, getListItems().length);

      // Issue query that does not match any language.
      searchInput.setValue('egaugnal');
      Polymer.dom.flush();
      assertEquals(0, getListItems().length);
    });

    test('has escape key behavior working correctly', function() {
      const searchInput = dialog.$$('cr-search-field');
      searchInput.setValue('dummyquery');

      // Test that dialog is not closed if 'Escape' is pressed on the input
      // and a search query exists.
      MockInteractions.keyDownOn(searchInput, 19, [], 'Escape');
      assertTrue(dialog.$.dialog.open);

      // Test that dialog is closed if 'Escape' is pressed on the input and no
      // search query exists.
      searchInput.setValue('');
      MockInteractions.keyDownOn(searchInput, 19, [], 'Escape');
      assertFalse(dialog.$.dialog.open);
    });

    test('languages are sorted on native display name', function() {
      // See https://crbug.com/1184064 for more details.
      // We can't test whether the order is *deterministic* w.r.t. device
      // language, as changing device language is not possible in a test, so we
      // do the next best thing and check if it's sorted on native display name.

      /**
       * @param {string} text
       * @return {string}
       */
      function getNativeDisplayName(text) {
        return text.includes(' - ') ? text.split(' - ')[0] : text;
      }

      const items = getListItems();
      const nativeDisplayNames =
          items.map(item => getNativeDisplayName(item.textContent.trim()));

      const sortedNativeDisplayNames =
          [...nativeDisplayNames].sort((a, b) => a.localeCompare(b, 'en'));
      assertDeepEquals(nativeDisplayNames, sortedNativeDisplayNames);
    });
  });

  suite('records metrics', () => {
    test('when adding languages', async () => {
      languagesPage.$$('#addLanguages').click();
      Polymer.dom.flush();
      await metricsProxy.whenCalled('recordAddLanguages');
    });

    test('when disabling translate.enable toggle', async () => {
      languagesPage.setPrefValue('translate.enabled', true);
      languagesPage.$$('#offerTranslation').click();
      Polymer.dom.flush();

      assertFalse(await metricsProxy.whenCalled('recordToggleTranslate'));
    });

    test('when enabling translate.enable toggle', async () => {
      languagesPage.setPrefValue('translate.enabled', false);
      languagesPage.$$('#offerTranslation').click();
      Polymer.dom.flush();

      assertTrue(await metricsProxy.whenCalled('recordToggleTranslate'));
    });

    test('when clicking on Manage Google Account language', async () => {
      // This test requires Language Settings V2 Update 2 to be enabled.
      languagesPage.languageSettingsV2Update2Enabled_ = true;
      loadTimeData.overrideValues({enableLanguageSettingsV2Update2: true});
      Polymer.dom.flush();

      // The below would normally create a new window using `window.open`, which
      // would change the focus from this test to the new window.
      // Prevent this from happening by overriding `window.open`.
      window.open = () => {};
      languagesPage.$$('#manageGoogleAccountLanguage').click();
      Polymer.dom.flush();
      assertEquals(
          await metricsProxy.whenCalled('recordInteraction'),
          LanguagesPageInteraction.OPEN_MANAGE_GOOGLE_ACCOUNT_LANGUAGE);
    });

    test('when clicking on "learn more" about web languages', async () => {
      // This test requires Update 2 to be disabled.
      languagesPage.languageSettingsV2Update2Enabled_ = false;
      loadTimeData.overrideValues({enableLanguageSettingsV2Update2: false});
      Polymer.dom.flush();

      const anchor = languagesPage.$$('#webLanguagesDescription').$$('a');
      // The below would normally create a new window, which would change the
      // focus from this test to the new window.
      // Prevent this from happening by adding an event listener on the anchor
      // element which stops the default behaviour (of opening a new window).
      anchor.addEventListener('click', (e) => e.preventDefault());
      anchor.click();
      assertEquals(
          await metricsProxy.whenCalled('recordInteraction'),
          LanguagesPageInteraction.OPEN_WEB_LANGUAGES_LEARN_MORE);
    });

    test('when clicking on "learn more" about web languages U2', async () => {
      // This test requires Update 2 to be enabled.
      languagesPage.languageSettingsV2Update2Enabled_ = true;
      loadTimeData.overrideValues({enableLanguageSettingsV2Update2: true});
      Polymer.dom.flush();

      const anchor = languagesPage.$$('#webLanguagesDescription').$$('a');
      // The below would normally create a new window, which would change the
      // focus from this test to the new window.
      // Prevent this from happening by adding an event listener on the anchor
      // element which stops the default behaviour (of opening a new window).
      anchor.addEventListener('click', (e) => e.preventDefault());
      anchor.click();
      assertEquals(
          await metricsProxy.whenCalled('recordInteraction'),
          LanguagesPageInteraction.OPEN_WEB_LANGUAGES_LEARN_MORE);
    });
  });
});

suite('change device language button', () => {
  setup(() => {
    document.body.innerHTML = '';
  });

  test('is hidden for guest users', () => {
    loadTimeData.overrideValues({
      isGuest: true,
    });
    const page = document.createElement('os-settings-languages-page-v2');
    document.body.appendChild(page);
    Polymer.dom.flush();

    assertFalse(!!page.$$('#changeDeviceLanguage'));
    assertFalse(!!page.$$('#changeDeviceLanguagePolicyIndicator'));
  });

  test('is disabled for secondary users', () => {
    loadTimeData.overrideValues(
        {isGuest: false, isSecondaryUser: true, primaryUserEmail: 'test.com'});
    const page = document.createElement('os-settings-languages-page-v2');
    document.body.appendChild(page);
    Polymer.dom.flush();

    const changeDeviceLanguageButton = page.$$('#changeDeviceLanguage');
    assertTrue(changeDeviceLanguageButton.disabled);
    assertFalse(changeDeviceLanguageButton.hidden);

    const changeDeviceLanguagePolicyIndicator =
        page.$$('#changeDeviceLanguagePolicyIndicator');
    assertFalse(changeDeviceLanguagePolicyIndicator.hidden);
    assertEquals(
        'test.com', changeDeviceLanguagePolicyIndicator.indicatorSourceName);
  });

  test('is enabled for primary users', () => {
    loadTimeData.overrideValues({
      isGuest: false,
      isSecondaryUser: false,
    });
    const page = document.createElement('os-settings-languages-page-v2');
    document.body.appendChild(page);
    Polymer.dom.flush();

    const changeDeviceLanguageButton = page.$$('#changeDeviceLanguage');
    assertFalse(changeDeviceLanguageButton.disabled);
    assertFalse(changeDeviceLanguageButton.hidden);

    assertFalse(!!page.$$('#changeDeviceLanguagePolicyIndicator'));
  });
});
