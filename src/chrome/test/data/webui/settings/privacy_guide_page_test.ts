// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {CookiePrimarySetting, PrivacyGuideCompletionFragmentElement, PrivacyGuideHistorySyncFragmentElement, PrivacyGuideStep, PrivacyGuideWelcomeFragmentElement, SafeBrowsingSetting, SettingsPrivacyGuidePageElement, SettingsRadioGroupElement} from 'chrome://settings/lazy_load.js';
import {MetricsBrowserProxyImpl, PrivacyGuideInteractions, PrivacyGuideSettingsStates, Router, routes, StatusAction, SyncBrowserProxyImpl, SyncPrefs, syncPrefsIndividualDataTypes, SyncStatus} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, flushTasks, isChildVisible} from 'chrome://webui-test/test_util.js';
import {getSyncAllPrefs} from './sync_test_util.js';

import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestSyncBrowserProxy} from './test_sync_browser_proxy.js';

// clang-format on

/* Maximum number of steps in the privacy guide, excluding the welcome and
 * completion steps.
 */
const PRIVACY_GUIDE_STEPS = 4;

/**
 * Equivalent of the user manually navigating to the corresponding step via
 * typing the URL and step parameter in the Omnibox.
 */
function navigateToStep(step: PrivacyGuideStep) {
  Router.getInstance().navigateTo(
      routes.PRIVACY_GUIDE,
      /* opt_dynamicParameters */ new URLSearchParams('step=' + step));
  flush();
}

/**
 * Fire a sign in status change event and flush the UI.
 */
function setSignInState(signedIn: boolean) {
  const event = {
    signedIn: signedIn,
  };
  webUIListenerCallback('update-sync-state', event);
  flush();
}

/**
 * Set all relevant sync status and fire a changed event and flush the UI.
 */
function setupSync({
  syncBrowserProxy,
  syncOn,
  syncAllDataTypes,
  typedUrlsSynced,
}: {
  syncBrowserProxy: TestSyncBrowserProxy,
  syncAllDataTypes: boolean,
  typedUrlsSynced: boolean,
  syncOn: boolean,
}) {
  if (syncAllDataTypes) {
    assertTrue(typedUrlsSynced);
  }
  if (typedUrlsSynced) {
    assertTrue(syncOn);
  }
  syncBrowserProxy.testSyncStatus = {
    signedIn: syncOn,
    hasError: false,
    statusAction: StatusAction.NO_ACTION,
  };
  webUIListenerCallback('sync-status-changed', syncBrowserProxy.testSyncStatus);

  const event = getSyncAllPrefs();
  // Overwrite datatypes needed in tests.
  event.syncAllDataTypes = syncAllDataTypes;
  event.typedUrlsSynced = typedUrlsSynced;
  webUIListenerCallback('sync-prefs-changed', event);
  flush();
}

suite('PrivacyGuidePage', function() {
  let page: SettingsPrivacyGuidePageElement;
  let syncBrowserProxy: TestSyncBrowserProxy;
  let shouldShowCookiesCard: boolean;
  let shouldShowSafeBrowsingCard: boolean;
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;

  setup(function() {
    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);
    syncBrowserProxy = new TestSyncBrowserProxy();
    syncBrowserProxy.testSyncStatus = null;
    SyncBrowserProxyImpl.setInstance(syncBrowserProxy);

    document.body.innerHTML = '';
    page = document.createElement('settings-privacy-guide-page');
    page.disableAnimationsForTesting();
    page.prefs = {
      privacy_guide: {
        viewed: {
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        },
      },
      url_keyed_anonymized_data_collection: {
        enabled: {
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: true,
        },
      },
      generated: {
        cookie_primary_setting: {
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: CookiePrimarySetting.BLOCK_THIRD_PARTY,
        },
        safe_browsing: {
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: SafeBrowsingSetting.STANDARD,
        },
      },
    };
    document.body.appendChild(page);
    shouldShowCookiesCard = true;
    shouldShowSafeBrowsingCard = true;

    // Simulates the route of the user entering the privacy guide from the S&P
    // settings. This is necessary as tests seem to by default define the
    // previous route as Settings "/". On a back navigation, "/" matches the
    // criteria for a valid Settings parent no matter how deep the subpage is in
    // the Settings tree. This would always navigate to Settings "/" instead of
    // to the parent of the current subpage.
    Router.getInstance().navigateTo(routes.PRIVACY);

    return flushTasks();
  });

  teardown(function() {
    page.remove();
    // Reset route to default. The route is updated as we navigate through the
    // cards, but the browser instance is shared among the tests, so otherwise
    // the next test will be initialized to the same card as the previous test.
    Router.getInstance().navigateTo(routes.BASIC);
  });

  /**
   * Returns a new promise that resolves after a window 'popstate' event.
   */
  function whenPopState(causeEvent: () => void): Promise<void> {
    const promise = eventToPromise('popstate', window);
    causeEvent();
    return promise;
  }


  function assertQueryParameter(step: PrivacyGuideStep) {
    assertEquals(step, Router.getInstance().getQueryParameters().get('step'));
  }

  function shouldShowHistorySyncCard(): boolean {
    return !syncBrowserProxy.testSyncStatus ||
        !!syncBrowserProxy.testSyncStatus.signedIn;
  }

  /**
   * Set the cookies setting for the privacy guide.
   */
  function setCookieSetting(setting: CookiePrimarySetting) {
    page.set('prefs.generated.cookie_primary_setting', {
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: setting,
    });
    shouldShowCookiesCard =
        setting === CookiePrimarySetting.BLOCK_THIRD_PARTY ||
        setting === CookiePrimarySetting.BLOCK_THIRD_PARTY_INCOGNITO;
    flush();
  }

  /**
   * Set the safe browsing setting for the privacy guide.
   */
  function setSafeBrowsingSetting(setting: SafeBrowsingSetting) {
    page.set('prefs.generated.safe_browsing', {
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: setting,
    });
    shouldShowSafeBrowsingCard = setting === SafeBrowsingSetting.ENHANCED ||
        setting === SafeBrowsingSetting.STANDARD;
    flush();
  }

  type AssertCardComponentsVisibleParams = {
    isSettingFooterVisibleExpected?: boolean,
    isBackButtonVisibleExpected?: boolean,
    isWelcomeFragmentVisibleExpected?: boolean,
    isCompletionFragmentVisibleExpected?: boolean,
    isMsbbFragmentVisibleExpected?: boolean,
    isClearOnExitFragmentVisibleExpected?: boolean,
    isHistorySyncFragmentVisibleExpected?: boolean,
    isSafeBrowsingFragmentVisibleExpected?: boolean,
    isCookiesFragmentVisibleExpected?: boolean,
  };

  function assertCardComponentsVisible({
    isSettingFooterVisibleExpected,
    isBackButtonVisibleExpected,
    isWelcomeFragmentVisibleExpected,
    isCompletionFragmentVisibleExpected,
    isMsbbFragmentVisibleExpected,
    isClearOnExitFragmentVisibleExpected,
    isHistorySyncFragmentVisibleExpected,
    isSafeBrowsingFragmentVisibleExpected,
    isCookiesFragmentVisibleExpected,
  }: AssertCardComponentsVisibleParams) {
    assertEquals(
        !!isSettingFooterVisibleExpected,
        isChildVisible(page, '#settingFooter'));
    if (isSettingFooterVisibleExpected) {
      const backButtonVisibility =
          getComputedStyle(
              page.shadowRoot!.querySelector<HTMLElement>('#backButton')!)
              .visibility;
      assertEquals(
          isBackButtonVisibleExpected ? 'visible' : 'hidden',
          backButtonVisibility);
    }
    assertEquals(
        !!isWelcomeFragmentVisibleExpected,
        isChildVisible(page, '#' + PrivacyGuideStep.WELCOME));
    assertEquals(
        !!isCompletionFragmentVisibleExpected,
        isChildVisible(page, '#' + PrivacyGuideStep.COMPLETION));
    assertEquals(
        !!isMsbbFragmentVisibleExpected,
        isChildVisible(page, '#' + PrivacyGuideStep.MSBB));
    assertEquals(
        !!isClearOnExitFragmentVisibleExpected,
        isChildVisible(page, '#' + PrivacyGuideStep.CLEAR_ON_EXIT));
    assertEquals(
        !!isHistorySyncFragmentVisibleExpected,
        isChildVisible(page, '#' + PrivacyGuideStep.HISTORY_SYNC));
    assertEquals(
        !!isSafeBrowsingFragmentVisibleExpected,
        isChildVisible(page, '#' + PrivacyGuideStep.SAFE_BROWSING));
    assertEquals(
        !!isCookiesFragmentVisibleExpected,
        isChildVisible(page, '#' + PrivacyGuideStep.COOKIES));
  }

  /**
   * @return The expected total number of active cards for the step indicator.
   */
  function getExpectedNumberOfActiveCards() {
    let numSteps = PRIVACY_GUIDE_STEPS;
    if (!shouldShowHistorySyncCard()) {
      numSteps -= 1;
    }
    if (!shouldShowCookiesCard) {
      numSteps -= 1;
    }
    if (!shouldShowSafeBrowsingCard) {
      numSteps -= 1;
    }
    return numSteps;
  }

  function assertStepIndicatorModel(activeIndex: number) {
    const model = page.computeStepIndicatorModel();
    assertEquals(activeIndex, model.active);
    assertEquals(getExpectedNumberOfActiveCards(), model.total);
  }

  function assertWelcomeCardVisible() {
    assertQueryParameter(PrivacyGuideStep.WELCOME);
    assertCardComponentsVisible({
      isWelcomeFragmentVisibleExpected: true,
    });
  }

  function assertCompletionCardVisible() {
    assertQueryParameter(PrivacyGuideStep.COMPLETION);
    assertCardComponentsVisible({
      isCompletionFragmentVisibleExpected: true,
    });
  }

  function assertMsbbCardVisible() {
    assertQueryParameter(PrivacyGuideStep.MSBB);
    assertCardComponentsVisible({
      isSettingFooterVisibleExpected: true,
      isBackButtonVisibleExpected: true,
      isMsbbFragmentVisibleExpected: true,
    });
    assertStepIndicatorModel(0);
  }

  function assertHistorySyncCardVisible() {
    assertQueryParameter(PrivacyGuideStep.HISTORY_SYNC);
    assertCardComponentsVisible({
      isSettingFooterVisibleExpected: true,
      isBackButtonVisibleExpected: true,
      isHistorySyncFragmentVisibleExpected: true,
    });
    assertStepIndicatorModel(1);
  }

  function assertSafeBrowsingCardVisible() {
    assertQueryParameter(PrivacyGuideStep.SAFE_BROWSING);
    assertCardComponentsVisible({
      isSettingFooterVisibleExpected: true,
      isBackButtonVisibleExpected: true,
      isSafeBrowsingFragmentVisibleExpected: true,
    });
    assertStepIndicatorModel(shouldShowHistorySyncCard() ? 2 : 1);
  }

  function assertCookiesCardVisible() {
    assertQueryParameter(PrivacyGuideStep.COOKIES);
    assertCardComponentsVisible({
      isSettingFooterVisibleExpected: true,
      isBackButtonVisibleExpected: true,
      isCookiesFragmentVisibleExpected: true,
    });
    let activeIndex = 3;
    if (!shouldShowHistorySyncCard()) {
      activeIndex -= 1;
    }
    if (!shouldShowSafeBrowsingCard) {
      activeIndex -= 1;
    }
    assertStepIndicatorModel(activeIndex);
  }

  test('startPrivacyGuide', function() {
    // Navigating to the privacy guide without a step parameter navigates to
    // the welcome card.
    Router.getInstance().navigateTo(routes.PRIVACY_GUIDE);
    flush();
    assertWelcomeCardVisible();

    assertTrue(page.getPref('privacy_guide.viewed').value);
  });

  test('welcomeForwardNavigation', async function() {
    // Navigating to the privacy guide without a step parameter navigates to
    // the welcome card.
    Router.getInstance().navigateTo(routes.PRIVACY_GUIDE);
    flush();
    assertWelcomeCardVisible();

    const welcomeFragment =
        page.shadowRoot!.querySelector<PrivacyGuideWelcomeFragmentElement>(
            '#' + PrivacyGuideStep.WELCOME)!;
    welcomeFragment.$.startButton.click();
    flush();
    assertMsbbCardVisible();

    const result = await testMetricsBrowserProxy.whenCalled(
        'recordPrivacyGuideNextNavigationHistogram');
    assertEquals(PrivacyGuideInteractions.WELCOME_NEXT_BUTTON, result);

    const actionResult =
        await testMetricsBrowserProxy.whenCalled('recordAction');
    assertEquals(actionResult, 'Settings.PrivacyGuide.NextClickWelcome');

    setupSync({
      syncBrowserProxy: syncBrowserProxy,
      syncOn: true,
      syncAllDataTypes: true,
      typedUrlsSynced: true,
    });
    assertMsbbCardVisible();
  });

  test('msbbBackNavigation', async function() {
    navigateToStep(PrivacyGuideStep.MSBB);
    assertMsbbCardVisible();

    page.shadowRoot!.querySelector<HTMLElement>('#backButton')!.click();
    flush();
    assertWelcomeCardVisible();

    const actionResult =
        await testMetricsBrowserProxy.whenCalled('recordAction');
    assertEquals(actionResult, 'Settings.PrivacyGuide.BackClickMSBB');
  });

  test('msbbForwardNavigationSyncOn', async function() {
    navigateToStep(PrivacyGuideStep.MSBB);
    setupSync({
      syncBrowserProxy: syncBrowserProxy,
      syncOn: true,
      syncAllDataTypes: true,
      typedUrlsSynced: true,
    });
    assertMsbbCardVisible();

    page.shadowRoot!.querySelector<HTMLElement>('#nextButton')!.click();
    assertHistorySyncCardVisible();

    const result = await testMetricsBrowserProxy.whenCalled(
        'recordPrivacyGuideNextNavigationHistogram');
    assertEquals(PrivacyGuideInteractions.MSBB_NEXT_BUTTON, result);

    const actionResult =
        await testMetricsBrowserProxy.whenCalled('recordAction');
    assertEquals(actionResult, 'Settings.PrivacyGuide.NextClickMSBB');
  });

  test('msbbForwardNavigationSyncOff', function() {
    navigateToStep(PrivacyGuideStep.MSBB);
    setupSync({
      syncBrowserProxy: syncBrowserProxy,
      syncOn: false,
      syncAllDataTypes: false,
      typedUrlsSynced: false,
    });
    assertMsbbCardVisible();

    page.shadowRoot!.querySelector<HTMLElement>('#nextButton')!.click();
    assertSafeBrowsingCardVisible();
  });

  test('historySyncBackNavigation', async function() {
    navigateToStep(PrivacyGuideStep.HISTORY_SYNC);
    setupSync({
      syncBrowserProxy: syncBrowserProxy,
      syncOn: true,
      syncAllDataTypes: true,
      typedUrlsSynced: true,
    });
    assertHistorySyncCardVisible();

    page.shadowRoot!.querySelector<HTMLElement>('#backButton')!.click();
    assertMsbbCardVisible();

    const actionResult =
        await testMetricsBrowserProxy.whenCalled('recordAction');
    assertEquals(actionResult, 'Settings.PrivacyGuide.BackClickHistorySync');
  });

  test('historySyncNavigatesAwayOnSyncOff', function() {
    navigateToStep(PrivacyGuideStep.HISTORY_SYNC);
    setupSync({
      syncBrowserProxy: syncBrowserProxy,
      syncOn: true,
      syncAllDataTypes: true,
      typedUrlsSynced: true,
    });
    assertHistorySyncCardVisible();

    // User disables sync while history sync card is shown.
    setupSync({
      syncBrowserProxy: syncBrowserProxy,
      syncOn: false,
      syncAllDataTypes: false,
      typedUrlsSynced: false,
    });
    assertSafeBrowsingCardVisible();
  });

  test('historySyncNotReachableWhenSyncOff', function() {
    setupSync({
      syncBrowserProxy: syncBrowserProxy,
      syncOn: true,
      syncAllDataTypes: true,
      typedUrlsSynced: true,
    });
    navigateToStep(PrivacyGuideStep.HISTORY_SYNC);
    setupSync({
      syncBrowserProxy: syncBrowserProxy,
      syncOn: false,
      syncAllDataTypes: false,
      typedUrlsSynced: false,
    });
    assertSafeBrowsingCardVisible();
  });

  test(
      'historySyncCardForwardNavigationShouldShowSafeBrowsingCard',
      async function() {
        navigateToStep(PrivacyGuideStep.HISTORY_SYNC);
        setupSync({
          syncBrowserProxy: syncBrowserProxy,
          syncOn: true,
          syncAllDataTypes: true,
          typedUrlsSynced: true,
        });
        setSafeBrowsingSetting(SafeBrowsingSetting.ENHANCED);
        setCookieSetting(CookiePrimarySetting.BLOCK_THIRD_PARTY);
        assertHistorySyncCardVisible();

        page.shadowRoot!.querySelector<HTMLElement>('#nextButton')!.click();
        assertSafeBrowsingCardVisible();

        const result = await testMetricsBrowserProxy.whenCalled(
            'recordPrivacyGuideNextNavigationHistogram');
        assertEquals(PrivacyGuideInteractions.HISTORY_SYNC_NEXT_BUTTON, result);

        const actionResult =
            await testMetricsBrowserProxy.whenCalled('recordAction');
        assertEquals(
            actionResult, 'Settings.PrivacyGuide.NextClickHistorySync');
      });

  test(
      'historySyncCardForwardNavigationShouldHideSafeBrowsingCard', function() {
        navigateToStep(PrivacyGuideStep.HISTORY_SYNC);
        setupSync({
          syncBrowserProxy: syncBrowserProxy,
          syncOn: true,
          syncAllDataTypes: true,
          typedUrlsSynced: true,
        });
        setSafeBrowsingSetting(SafeBrowsingSetting.DISABLED);
        setCookieSetting(CookiePrimarySetting.BLOCK_THIRD_PARTY);
        assertHistorySyncCardVisible();

        page.shadowRoot!.querySelector<HTMLElement>('#nextButton')!.click();
        assertCookiesCardVisible();
      });

  test('safeBrowsingCardBackNavigationSyncOn', async function() {
    navigateToStep(PrivacyGuideStep.SAFE_BROWSING);
    setupSync({
      syncBrowserProxy: syncBrowserProxy,
      syncOn: true,
      syncAllDataTypes: true,
      typedUrlsSynced: true,
    });
    assertSafeBrowsingCardVisible();

    page.shadowRoot!.querySelector<HTMLElement>('#backButton')!.click();
    assertHistorySyncCardVisible();

    const actionResult =
        await testMetricsBrowserProxy.whenCalled('recordAction');
    assertEquals(actionResult, 'Settings.PrivacyGuide.BackClickSafeBrowsing');
  });

  test('safeBrowsingCardBackNavigationSyncOff', async function() {
    navigateToStep(PrivacyGuideStep.SAFE_BROWSING);
    setupSync({
      syncBrowserProxy: syncBrowserProxy,
      syncOn: false,
      syncAllDataTypes: false,
      typedUrlsSynced: false,
    });
    assertSafeBrowsingCardVisible();

    page.shadowRoot!.querySelector<HTMLElement>('#backButton')!.click();
    assertMsbbCardVisible();
  });

  test('safeBrowsingCardGetsUpdated', function() {
    navigateToStep(PrivacyGuideStep.SAFE_BROWSING);
    setSafeBrowsingSetting(SafeBrowsingSetting.ENHANCED);
    setCookieSetting(CookiePrimarySetting.BLOCK_THIRD_PARTY);
    assertSafeBrowsingCardVisible();
    const radioButtonGroup =
        page.shadowRoot!.querySelector('#' + PrivacyGuideStep.SAFE_BROWSING)!
            .shadowRoot!.querySelector<SettingsRadioGroupElement>(
                '#safeBrowsingRadioGroup')!;
    assertEquals(
        Number(radioButtonGroup.selected), SafeBrowsingSetting.ENHANCED);

    // Changing the safe browsing setting should automatically change the
    // selected radio button.
    setSafeBrowsingSetting(SafeBrowsingSetting.STANDARD);
    assertEquals(
        Number(radioButtonGroup.selected), SafeBrowsingSetting.STANDARD);

    // Changing the safe browsing setting to a disabled state while shown should
    // navigate away from the safe browsing card.
    setSafeBrowsingSetting(SafeBrowsingSetting.DISABLED);
    assertCookiesCardVisible();
  });

  test(
      'safeBrowsingCardForwardNavigationShouldShowCookiesCard',
      async function() {
        navigateToStep(PrivacyGuideStep.SAFE_BROWSING);
        setCookieSetting(CookiePrimarySetting.BLOCK_THIRD_PARTY);
        assertSafeBrowsingCardVisible();

        page.shadowRoot!.querySelector<HTMLElement>('#nextButton')!.click();
        flush();
        assertCookiesCardVisible();

        const result = await testMetricsBrowserProxy.whenCalled(
            'recordPrivacyGuideNextNavigationHistogram');
        assertEquals(
            PrivacyGuideInteractions.SAFE_BROWSING_NEXT_BUTTON, result);

        const actionResult =
            await testMetricsBrowserProxy.whenCalled('recordAction');
        assertEquals(
            actionResult, 'Settings.PrivacyGuide.NextClickSafeBrowsing');
      });

  test('safeBrowsingCardForwardNavigationShouldHideCookiesCard', function() {
    navigateToStep(PrivacyGuideStep.SAFE_BROWSING);
    setCookieSetting(CookiePrimarySetting.ALLOW_ALL);
    assertSafeBrowsingCardVisible();

    page.shadowRoot!.querySelector<HTMLElement>('#nextButton')!.click();
    flush();
    assertCompletionCardVisible();
  });

  test('cookiesCardBackNavigationShouldShowSafeBrowsingCard', async function() {
    navigateToStep(PrivacyGuideStep.COOKIES);
    setupSync({
      syncBrowserProxy: syncBrowserProxy,
      syncOn: true,
      syncAllDataTypes: true,
      typedUrlsSynced: true,
    });
    setSafeBrowsingSetting(SafeBrowsingSetting.STANDARD);
    assertCookiesCardVisible();

    page.shadowRoot!.querySelector<HTMLElement>('#backButton')!.click();
    flush();
    assertSafeBrowsingCardVisible();

    const actionResult =
        await testMetricsBrowserProxy.whenCalled('recordAction');
    assertEquals(actionResult, 'Settings.PrivacyGuide.BackClickCookies');
  });

  test('cookiesCardBackNavigationShouldHideSafeBrowsingCard', function() {
    navigateToStep(PrivacyGuideStep.COOKIES);
    setupSync({
      syncBrowserProxy: syncBrowserProxy,
      syncOn: true,
      syncAllDataTypes: true,
      typedUrlsSynced: true,
    });
    setSafeBrowsingSetting(SafeBrowsingSetting.DISABLED);
    assertCookiesCardVisible();

    page.shadowRoot!.querySelector<HTMLElement>('#backButton')!.click();
    flush();
    assertHistorySyncCardVisible();
  });

  test('cookiesCardForwardNavigation', async function() {
    navigateToStep(PrivacyGuideStep.COOKIES);
    assertCookiesCardVisible();

    page.shadowRoot!.querySelector<HTMLElement>('#nextButton')!.click();
    flush();
    assertCompletionCardVisible();

    const result = await testMetricsBrowserProxy.whenCalled(
        'recordPrivacyGuideNextNavigationHistogram');
    assertEquals(PrivacyGuideInteractions.COOKIES_NEXT_BUTTON, result);

    const actionResult =
        await testMetricsBrowserProxy.whenCalled('recordAction');
    assertEquals(actionResult, 'Settings.PrivacyGuide.NextClickCookies');
  });

  test('cookiesCardGetsUpdated', function() {
    navigateToStep(PrivacyGuideStep.COOKIES);
    setCookieSetting(CookiePrimarySetting.BLOCK_THIRD_PARTY);
    assertCookiesCardVisible();
    const radioButtonGroup =
        page.shadowRoot!.querySelector('#' + PrivacyGuideStep.COOKIES)!
            .shadowRoot!.querySelector<SettingsRadioGroupElement>(
                '#cookiesRadioGroup')!;
    assertEquals(
        Number(radioButtonGroup.selected),
        CookiePrimarySetting.BLOCK_THIRD_PARTY);

    // Changing the cookie setting should automatically change the selected
    // radio button.
    setCookieSetting(CookiePrimarySetting.BLOCK_THIRD_PARTY_INCOGNITO);
    assertEquals(
        Number(radioButtonGroup.selected),
        CookiePrimarySetting.BLOCK_THIRD_PARTY_INCOGNITO);

    // Changing the cookie setting to a non-third-party state while shown should
    // navigate away from the cookies card.
    setCookieSetting(CookiePrimarySetting.ALLOW_ALL);
    assertCompletionCardVisible();
  });

  test('completionCardBackNavigation', async function() {
    navigateToStep(PrivacyGuideStep.COMPLETION);
    setCookieSetting(CookiePrimarySetting.BLOCK_THIRD_PARTY);
    assertCompletionCardVisible();

    const completionFragment =
        page.shadowRoot!.querySelector('#' + PrivacyGuideStep.COMPLETION)!;
    completionFragment.shadowRoot!.querySelector<HTMLElement>(
                                      '#backButton')!.click();
    flush();
    assertCookiesCardVisible();

    const actionResult =
        await testMetricsBrowserProxy.whenCalled('recordAction');
    assertEquals(actionResult, 'Settings.PrivacyGuide.BackClickCompletion');
  });

  test('completionCardBackToSettingsNavigation', function() {
    navigateToStep(PrivacyGuideStep.COMPLETION);
    assertCompletionCardVisible();

    return whenPopState(async function() {
             const completionFragment = page.shadowRoot!.querySelector(
                 '#' + PrivacyGuideStep.COMPLETION)!;
             completionFragment.shadowRoot!
                 .querySelector<HTMLElement>('#leaveButton')!.click();

             const result = await testMetricsBrowserProxy.whenCalled(
                 'recordPrivacyGuideNextNavigationHistogram');
             assertEquals(
                 PrivacyGuideInteractions.COMPLETION_NEXT_BUTTON, result);

             const actionResult =
                 await testMetricsBrowserProxy.whenCalled('recordAction');
             assertEquals(
                 actionResult, 'Settings.PrivacyGuide.NextClickCompletion');
           })
        .then(function() {
          assertEquals(routes.PRIVACY, Router.getInstance().getCurrentRoute());
        });
  });

  test('privacyGuideVisibilityChildAccount', function() {
    // Set the user to have a non-child account.
    const syncStatus:
        SyncStatus = {childUser: false, statusAction: StatusAction.NO_ACTION};
    webUIListenerCallback('sync-status-changed', syncStatus);
    flush();

    // Navigating to the privacy guide works.
    Router.getInstance().navigateTo(routes.PRIVACY_GUIDE);
    flush();
    assertWelcomeCardVisible();

    // The user signs in to a child user account. This hides the privacy guide
    // and navigates away back to privacy settings page.
    const newSyncStatus:
        SyncStatus = {childUser: true, statusAction: StatusAction.NO_ACTION};
    webUIListenerCallback('sync-status-changed', newSyncStatus);
    flush();
    assertEquals(routes.PRIVACY, Router.getInstance().getCurrentRoute());

    // User trying to manually navigate to privacy guide fails.
    Router.getInstance().navigateTo(routes.PRIVACY_GUIDE);
    flush();
    assertEquals(routes.PRIVACY, Router.getInstance().getCurrentRoute());
  });

  test('privacyGuideVisibilityManagedAccount', function() {
    // Set the user to have a non-managed account.
    webUIListenerCallback('is-managed-changed', false);
    flush();

    // Navigating to the privacy guide works.
    Router.getInstance().navigateTo(routes.PRIVACY_GUIDE);
    flush();
    assertWelcomeCardVisible();

    // The user signs in to a managed account. This hides the privacy guide and
    // navigates away back to privacy settings page.
    webUIListenerCallback('is-managed-changed', true);
    flush();
    assertEquals(routes.PRIVACY, Router.getInstance().getCurrentRoute());

    // User trying to manually navigate to privacy guide fails.
    Router.getInstance().navigateTo(routes.PRIVACY_GUIDE);
    flush();
    assertEquals(routes.PRIVACY, Router.getInstance().getCurrentRoute());
  });
});

suite('PrivacyGuideFragmentMetrics', function() {
  let page: SettingsPrivacyGuidePageElement;
  let syncBrowserProxy: TestSyncBrowserProxy;
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;

  setup(function() {
    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);
    syncBrowserProxy = new TestSyncBrowserProxy();
    syncBrowserProxy.testSyncStatus = null;
    SyncBrowserProxyImpl.setInstance(syncBrowserProxy);

    document.body.innerHTML = '';
    page = document.createElement('settings-privacy-guide-page');
    page.disableAnimationsForTesting();
    page.prefs = {
      privacy_guide: {
        viewed: {
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        },
      },
      url_keyed_anonymized_data_collection: {
        enabled: {
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: true,
        },
      },
      generated: {
        cookie_primary_setting: {
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: CookiePrimarySetting.BLOCK_THIRD_PARTY,
        },
        safe_browsing: {
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: SafeBrowsingSetting.STANDARD,
        },
      },
    };
    document.body.appendChild(page);

    // Simulates the route of the user entering the privacy guide from the S&P
    // settings. This is necessary as tests seem to by default define the
    // previous route as Settings "/". On a back navigation, "/" matches the
    // criteria for a valid Settings parent no matter how deep the subpage is in
    // the Settings tree. This would always navigate to Settings "/" instead of
    // to the parent of the current subpage.
    Router.getInstance().navigateTo(routes.PRIVACY);

    return flushTasks();
  });

  teardown(function() {
    page.remove();
    // Reset route to default. The route is updated as we navigate through the
    // cards, but the browser instance is shared among the tests, so otherwise
    // the next test will be initialized to the same card as the previous test.
    Router.getInstance().navigateTo(routes.BASIC);
  });

  async function assertMsbbMetrics({
    msbbStartOn,
    changeSetting,
    expectedMetric,
  }: {
    msbbStartOn: boolean,
    changeSetting: boolean,
    expectedMetric: PrivacyGuideSettingsStates,
  }) {
    page.setPrefValue(
        'url_keyed_anonymized_data_collection.enabled', msbbStartOn);
    flush();
    navigateToStep(PrivacyGuideStep.MSBB);

    if (changeSetting) {
      page.shadowRoot!.querySelector('#' + PrivacyGuideStep.MSBB)!.shadowRoot!
          .querySelector<HTMLElement>('#urlCollectionToggle')!.click();
      flush();
      const actionResult =
          await testMetricsBrowserProxy.whenCalled('recordAction');
      assertEquals(
          actionResult,
          msbbStartOn ? 'Settings.PrivacyGuide.ChangeMSBBOff' :
                        'Settings.PrivacyGuide.ChangeMSBBOn');
    }

    // Go back instead of forward to not need sync state in the test.
    page.shadowRoot!.querySelector<HTMLElement>('#backButton')!.click();
    flush();

    const result = await testMetricsBrowserProxy.whenCalled(
        'recordPrivacyGuideSettingsStatesHistogram');
    assertEquals(result, expectedMetric);
  }

  async function assertHistorySyncMetrics({
    historySyncStartOn,
    changeSetting,
    expectedMetric,
  }: {
    historySyncStartOn: boolean,
    changeSetting: boolean,
    expectedMetric: PrivacyGuideSettingsStates,
  }) {
    setupSync({
      syncBrowserProxy: syncBrowserProxy,
      syncOn: true,
      syncAllDataTypes: historySyncStartOn,
      typedUrlsSynced: historySyncStartOn,
    });
    navigateToStep(PrivacyGuideStep.HISTORY_SYNC);

    if (changeSetting) {
      page.shadowRoot!.querySelector('#' + PrivacyGuideStep.HISTORY_SYNC)!
          .shadowRoot!.querySelector<HTMLElement>('#historyToggle')!.click();
      flush();
      const actionResult =
          await testMetricsBrowserProxy.whenCalled('recordAction');
      assertEquals(
          actionResult,
          historySyncStartOn ? 'Settings.PrivacyGuide.ChangeHistorySyncOff' :
                               'Settings.PrivacyGuide.ChangeHistorySyncOn');
    }

    page.shadowRoot!.querySelector<HTMLElement>('#nextButton')!.click();
    flush();

    const result = await testMetricsBrowserProxy.whenCalled(
        'recordPrivacyGuideSettingsStatesHistogram');
    assertEquals(result, expectedMetric);
  }

  async function assertSafeBrowsingMetrics({
    safeBrowsingStartsEnhanced,
    changeSetting,
    expectedMetric,
  }: {
    safeBrowsingStartsEnhanced: boolean,
    changeSetting: boolean,
    expectedMetric: PrivacyGuideSettingsStates,
  }) {
    const safeBrowsingStartState = safeBrowsingStartsEnhanced ?
        SafeBrowsingSetting.ENHANCED :
        SafeBrowsingSetting.STANDARD;
    page.setPrefValue('generated.safe_browsing', safeBrowsingStartState);
    flush();
    navigateToStep(PrivacyGuideStep.SAFE_BROWSING);

    if (changeSetting) {
      page.shadowRoot!.querySelector(
                          '#' + PrivacyGuideStep.SAFE_BROWSING)!.shadowRoot!
          .querySelector<HTMLElement>(
              safeBrowsingStartsEnhanced ?
                  '#safeBrowsingRadioStandard' :
                  '#safeBrowsingRadioEnhanced')!.click();
      flush();
      const actionResult =
          await testMetricsBrowserProxy.whenCalled('recordAction');
      assertEquals(
          actionResult,
          safeBrowsingStartsEnhanced ?
              'Settings.PrivacyGuide.ChangeSafeBrowsingStandard' :
              'Settings.PrivacyGuide.ChangeSafeBrowsingEnhanced');
    }

    page.shadowRoot!.querySelector<HTMLElement>('#nextButton')!.click();
    flush();

    const result = await testMetricsBrowserProxy.whenCalled(
        'recordPrivacyGuideSettingsStatesHistogram');
    assertEquals(result, expectedMetric);
  }

  async function assertCookieMetrics({
    cookieStartsBlock3PIncognito,
    changeSetting,
    expectedMetric,
  }: {
    cookieStartsBlock3PIncognito: boolean,
    changeSetting: boolean,
    expectedMetric: PrivacyGuideSettingsStates,
  }) {
    const cookieStartState = cookieStartsBlock3PIncognito ?
        CookiePrimarySetting.BLOCK_THIRD_PARTY_INCOGNITO :
        CookiePrimarySetting.BLOCK_THIRD_PARTY;
    page.setPrefValue('generated.cookie_primary_setting', cookieStartState);
    flush();
    navigateToStep(PrivacyGuideStep.COOKIES);

    if (changeSetting) {
      page.shadowRoot!.querySelector(
                          '#' + PrivacyGuideStep.COOKIES)!.shadowRoot!
          .querySelector<HTMLElement>(
              cookieStartsBlock3PIncognito ? '#block3P' :
                                             '#block3PIncognito')!.click();
      flush();
      const actionResult =
          await testMetricsBrowserProxy.whenCalled('recordAction');
      assertEquals(
          actionResult,
          cookieStartsBlock3PIncognito ?
              'Settings.PrivacyGuide.ChangeCookiesBlock3P' :
              'Settings.PrivacyGuide.ChangeCookiesBlock3PIncognito');
    }

    page.shadowRoot!.querySelector<HTMLElement>('#nextButton')!.click();
    flush();

    const result = await testMetricsBrowserProxy.whenCalled(
        'recordPrivacyGuideSettingsStatesHistogram');
    assertEquals(result, expectedMetric);
  }

  test('msbbMetricsOnToOn', function() {
    return assertMsbbMetrics({
      msbbStartOn: true,
      changeSetting: false,
      expectedMetric: PrivacyGuideSettingsStates.MSBB_ON_TO_ON
    });
  });

  test('msbbMetricsOnToOff', function() {
    return assertMsbbMetrics({
      msbbStartOn: true,
      changeSetting: true,
      expectedMetric: PrivacyGuideSettingsStates.MSBB_ON_TO_OFF
    });
  });

  test('msbbMetricsOffToOn', function() {
    return assertMsbbMetrics({
      msbbStartOn: false,
      changeSetting: true,
      expectedMetric: PrivacyGuideSettingsStates.MSBB_OFF_TO_ON
    });
  });

  test('msbbMetricsOffToOff', function() {
    return assertMsbbMetrics({
      msbbStartOn: false,
      changeSetting: false,
      expectedMetric: PrivacyGuideSettingsStates.MSBB_OFF_TO_OFF
    });
  });

  test('historySyncOnToOn', function() {
    return assertHistorySyncMetrics({
      historySyncStartOn: true,
      changeSetting: false,
      expectedMetric: PrivacyGuideSettingsStates.HISTORY_SYNC_ON_TO_ON
    });
  });

  test('historySyncOnToOff', function() {
    return assertHistorySyncMetrics({
      historySyncStartOn: true,
      changeSetting: true,
      expectedMetric: PrivacyGuideSettingsStates.HISTORY_SYNC_ON_TO_OFF
    });
  });

  test('historySyncOffToOn', function() {
    return assertHistorySyncMetrics({
      historySyncStartOn: false,
      changeSetting: true,
      expectedMetric: PrivacyGuideSettingsStates.HISTORY_SYNC_OFF_TO_ON
    });
  });

  test('historySyncOffToOff', function() {
    return assertHistorySyncMetrics({
      historySyncStartOn: false,
      changeSetting: false,
      expectedMetric: PrivacyGuideSettingsStates.HISTORY_SYNC_OFF_TO_OFF
    });
  });

  test('safeBrowsingMetricsEnhancedToEnhanced', function() {
    return assertSafeBrowsingMetrics({
      safeBrowsingStartsEnhanced: true,
      changeSetting: false,
      expectedMetric:
          PrivacyGuideSettingsStates.SAFE_BROWSING_ENHANCED_TO_ENHANCED,
    });
  });

  test('safeBrowsingMetricsEnhancedToStandard', function() {
    return assertSafeBrowsingMetrics({
      safeBrowsingStartsEnhanced: true,
      changeSetting: true,
      expectedMetric:
          PrivacyGuideSettingsStates.SAFE_BROWSING_ENHANCED_TO_STANDARD,
    });
  });

  test('safeBrowsingMetricsStandardToEnhanced', function() {
    return assertSafeBrowsingMetrics({
      safeBrowsingStartsEnhanced: false,
      changeSetting: true,
      expectedMetric:
          PrivacyGuideSettingsStates.SAFE_BROWSING_STANDARD_TO_ENHANCED,
    });
  });

  test('safeBrowsingMetricsStandardToStandard', function() {
    return assertSafeBrowsingMetrics({
      safeBrowsingStartsEnhanced: false,
      changeSetting: false,
      expectedMetric:
          PrivacyGuideSettingsStates.SAFE_BROWSING_STANDARD_TO_STANDARD,
    });
  });

  test('cookiesMetrics3PIncognitoTo3PIncognito', function() {
    return assertCookieMetrics({
      cookieStartsBlock3PIncognito: true,
      changeSetting: false,
      expectedMetric:
          PrivacyGuideSettingsStates.BLOCK_3P_INCOGNITO_TO_3P_INCOGNITO,
    });
  });

  test('cookiesMetrics3PIncognitoTo3P', function() {
    return assertCookieMetrics({
      cookieStartsBlock3PIncognito: true,
      changeSetting: true,
      expectedMetric: PrivacyGuideSettingsStates.BLOCK_3P_INCOGNITO_TO_3P,
    });
  });

  test('cookiesMetrics3PTo3PIncognito', function() {
    return assertCookieMetrics({
      cookieStartsBlock3PIncognito: false,
      changeSetting: true,
      expectedMetric: PrivacyGuideSettingsStates.BLOCK_3P_TO_3P_INCOGNITO,
    });
  });

  test('cookiesMetrics3PTo3P', function() {
    return assertCookieMetrics({
      cookieStartsBlock3PIncognito: false,
      changeSetting: false,
      expectedMetric: PrivacyGuideSettingsStates.BLOCK_3P_TO_3P,
    });
  });
});

suite('HistorySyncFragment', function() {
  let page: PrivacyGuideHistorySyncFragmentElement;
  let syncBrowserProxy: TestSyncBrowserProxy;

  setup(function() {
    syncBrowserProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.setInstance(syncBrowserProxy);

    document.body.innerHTML = '';
    page = document.createElement('privacy-guide-history-sync-fragment');
    document.body.appendChild(page);
    return flushTasks();
  });

  teardown(function() {
    page.remove();
  });

  function setSyncStatus({
    syncAllDataTypes,
    typedUrlsSynced,
    passwordsSynced,
  }: {
    syncAllDataTypes: boolean,
    typedUrlsSynced: boolean,
    passwordsSynced: boolean,
  }) {
    if (syncAllDataTypes) {
      assertTrue(typedUrlsSynced);
      assertTrue(passwordsSynced);
    }
    const event: SyncPrefs = {} as unknown as SyncPrefs;
    for (const datatype of syncPrefsIndividualDataTypes) {
      (event as unknown as {[key: string]: boolean})[datatype] = true;
    }
    // Overwrite datatypes needed in tests.
    event.syncAllDataTypes = syncAllDataTypes;
    event.typedUrlsSynced = typedUrlsSynced;
    event.passwordsSynced = passwordsSynced;
    webUIListenerCallback('sync-prefs-changed', event);
    flush();
  }

  async function assertBrowserProxyCall({
    syncAllDatatypesExpected,
    typedUrlsSyncedExpected,
  }: {
    syncAllDatatypesExpected: boolean,
    typedUrlsSyncedExpected: boolean,
  }) {
    const syncPrefs = await syncBrowserProxy.whenCalled('setSyncDatatypes');
    assertEquals(syncAllDatatypesExpected, syncPrefs.syncAllDataTypes);
    assertEquals(typedUrlsSyncedExpected, syncPrefs.typedUrlsSynced);
    syncBrowserProxy.resetResolver('setSyncDatatypes');
  }

  test('syncAllOnDisableReenableHistorySync', async function() {
    setSyncStatus({
      syncAllDataTypes: true,
      typedUrlsSynced: true,
      passwordsSynced: true,
    });
    page.$.historyToggle.click();
    await assertBrowserProxyCall({
      syncAllDatatypesExpected: false,
      typedUrlsSyncedExpected: false,
    });

    // Re-enabling history sync re-enables sync all if sync all was on before
    // and if all sync datatypes are still enabled.
    page.$.historyToggle.click();
    return assertBrowserProxyCall({
      syncAllDatatypesExpected: true,
      typedUrlsSyncedExpected: true,
    });
  });

  test('syncAllOnDisableReenableHistorySyncOtherDatatypeOff', async function() {
    setSyncStatus({
      syncAllDataTypes: true,
      typedUrlsSynced: true,
      passwordsSynced: true,
    });
    page.$.historyToggle.click();
    await assertBrowserProxyCall({
      syncAllDatatypesExpected: false,
      typedUrlsSyncedExpected: false,
    });

    // The user disables another datatype in a different tab.
    setSyncStatus({
      syncAllDataTypes: false,
      typedUrlsSynced: false,
      passwordsSynced: false,
    });

    // Re-enabling history sync in the privacy guide doesn't re-enable sync
    // all.
    page.$.historyToggle.click();
    return assertBrowserProxyCall({
      syncAllDatatypesExpected: false,
      typedUrlsSyncedExpected: true,
    });
  });

  test('syncAllOnDisableReenableHistorySyncWithNavigation', async function() {
    setSyncStatus({
      syncAllDataTypes: true,
      typedUrlsSynced: true,
      passwordsSynced: true,
    });
    page.$.historyToggle.click();
    await assertBrowserProxyCall({
      syncAllDatatypesExpected: false,
      typedUrlsSyncedExpected: false,
    });

    // The user navigates to another card, then back to the history sync card.
    Router.getInstance().navigateTo(
        routes.PRIVACY_GUIDE,
        /* opt_dynamicParameters */ new URLSearchParams('step=msbb'));
    Router.getInstance().navigateTo(
        routes.PRIVACY_GUIDE,
        /* opt_dynamicParameters */ new URLSearchParams('step=historySync'));

    // Re-enabling history sync in the privacy guide doesn't re-enable sync
    // all.
    page.$.historyToggle.click();
    return assertBrowserProxyCall({
      syncAllDatatypesExpected: false,
      typedUrlsSyncedExpected: true,
    });
  });

  test('syncAllOffDisableReenableHistorySync', async function() {
    setSyncStatus({
      syncAllDataTypes: false,
      typedUrlsSynced: true,
      passwordsSynced: true,
    });
    page.$.historyToggle.click();
    await assertBrowserProxyCall({
      syncAllDatatypesExpected: false,
      typedUrlsSyncedExpected: false,
    });

    // Re-enabling history sync doesn't re-enable sync all if sync all wasn't on
    // originally.
    page.$.historyToggle.click();
    return assertBrowserProxyCall({
      syncAllDatatypesExpected: false,
      typedUrlsSyncedExpected: true,
    });
  });

  test('syncAllOffEnableHistorySync', function() {
    setSyncStatus({
      syncAllDataTypes: false,
      typedUrlsSynced: false,
      passwordsSynced: true,
    });
    page.$.historyToggle.click();
    return assertBrowserProxyCall({
      syncAllDatatypesExpected: false,
      typedUrlsSyncedExpected: true,
    });
  });
});

suite('CompletionFragment', function() {
  let page: PrivacyGuideCompletionFragmentElement;
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;

  setup(function() {
    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);

    document.body.innerHTML = '';
    page = document.createElement('privacy-guide-completion-fragment');
    document.body.appendChild(page);
    return flushTasks();
  });

  test('SWAALinkClick', async function() {
    setSignInState(true);

    assertTrue(isChildVisible(page, '#waaRow'));
    page.shadowRoot!.querySelector<HTMLElement>('#waaRow')!.click();
    flush();

    const result = await testMetricsBrowserProxy.whenCalled(
        'recordPrivacyGuideEntryExitHistogram');
    assertEquals(PrivacyGuideInteractions.SWAA_COMPLETION_LINK, result);
  });

  test('privacySandboxLinkClick', async function() {
    page.shadowRoot!.querySelector<HTMLElement>('#privacySandboxRow')!.click();
    flush();

    const result = await testMetricsBrowserProxy.whenCalled(
        'recordPrivacyGuideEntryExitHistogram');
    assertEquals(
        PrivacyGuideInteractions.PRIVACY_SANDBOX_COMPLETION_LINK, result);
  });

  test('updateFragmentFromSignIn', function() {
    setSignInState(true);
    assertTrue(isChildVisible(page, '#privacySandboxRow'));
    assertTrue(isChildVisible(page, '#waaRow'));

    // Sign the user out and expect the waa row to no longer be visible.
    setSignInState(false);
    assertTrue(isChildVisible(page, '#privacySandboxRow'));
    assertFalse(isChildVisible(page, '#waaRow'));
  });
});
