// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {isChromeOS} from 'chrome://resources/js/cr.m.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ClearBrowsingDataBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {Router, routes, StatusAction, SyncBrowserProxyImpl} from 'chrome://settings/settings.js';
import {TestClearBrowsingDataBrowserProxy} from 'chrome://test/settings/test_clear_browsing_data_browser_proxy.js';
import {TestSyncBrowserProxy} from 'chrome://test/settings/test_sync_browser_proxy.js';
import {eventToPromise, isChildVisible, isVisible, whenAttributeIs} from 'chrome://test/test_util.js';

// clang-format on

function getClearBrowsingDataPrefs() {
  return {
    browser: {
      clear_data: {
        browsing_history: {
          key: 'browser.clear_data.browsing_history',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        },
        browsing_history_basic: {
          key: 'browser.clear_data.browsing_history_basic',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        },
        cache: {
          key: 'browser.clear_data.cache',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        },
        cache_basic: {
          key: 'browser.clear_data.cache_basic',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        },
        cookies: {
          key: 'browser.clear_data.cookies',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        },
        cookies_basic: {
          key: 'browser.clear_data.cookies_basic',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        },
        download_history: {
          key: 'browser.clear_data.download_history',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        },
        hosted_apps_data: {
          key: 'browser.clear_data.hosted_apps_data',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        },
        form_data: {
          key: 'browser.clear_data.form_data',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        },
        passwords: {
          key: 'browser.clear_data.passwords',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        },
        site_settings: {
          key: 'browser.clear_data.site_settings',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        },
        time_period: {
          key: 'browser.clear_data.time_period',
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: 0,
        },
        time_period_basic: {
          key: 'browser.clear_data.time_period_basic',
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: 0,
        },
      },
      last_clear_browsing_data_tab: {
        key: 'browser.last_clear_browsing_data_tab',
        type: chrome.settingsPrivate.PrefType.NUMBER,
        value: 0,
      },
    }
  };
}

suite('ClearBrowsingDataDesktop', function() {
  /** @type {TestClearBrowsingDataBrowserProxy} */
  let testBrowserProxy;

  /** @type {TestSyncBrowserProxy} */
  let testSyncBrowserProxy;

  /** @type {SettingsClearBrowsingDataDialogElement} */
  let element;

  setup(function() {
    testBrowserProxy = new TestClearBrowsingDataBrowserProxy();
    ClearBrowsingDataBrowserProxyImpl.setInstance(testBrowserProxy);
    testSyncBrowserProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.setInstance(testSyncBrowserProxy);
    PolymerTest.clearBody();
    element = document.createElement('settings-clear-browsing-data-dialog');
    element.set('prefs', getClearBrowsingDataPrefs());
    document.body.appendChild(element);
    return testBrowserProxy.whenCalled('initialize').then(() => {
      assertTrue(
          element.shadowRoot.querySelector('#clearBrowsingDataDialog').open);
    });
  });

  teardown(function() {
    element.remove();
  });

  test('ClearBrowsingDataSyncAccountInfoDesktop', function() {
    // Not syncing: the footer is hidden.
    webUIListenerCallback('sync-status-changed', {
      signedIn: false,
      hasError: false,
    });
    flush();
    assertFalse(!!element.shadowRoot.querySelector(
        '#clearBrowsingDataDialog [slot=footer]'));

    // Syncing: the footer is shown, with the normal sync info.
    webUIListenerCallback('sync-status-changed', {
      signedIn: true,
      hasError: false,
    });
    flush();
    assertTrue(!!element.shadowRoot.querySelector(
        '#clearBrowsingDataDialog [slot=footer]'));
    assertTrue(isChildVisible(element, '#sync-info'));
    assertFalse(isChildVisible(element, '#sync-paused-info'));
    assertFalse(isChildVisible(element, '#sync-passphrase-error-info'));
    assertFalse(isChildVisible(element, '#sync-other-error-info'));

    // Sync is paused.
    webUIListenerCallback('sync-status-changed', {
      signedIn: true,
      hasError: true,
      statusAction: StatusAction.REAUTHENTICATE,
    });
    flush();
    assertFalse(isChildVisible(element, '#sync-info'));
    assertTrue(isChildVisible(element, '#sync-paused-info'));
    assertFalse(isChildVisible(element, '#sync-passphrase-error-info'));
    assertFalse(isChildVisible(element, '#sync-other-error-info'));

    // Sync passphrase error.
    webUIListenerCallback('sync-status-changed', {
      signedIn: true,
      hasError: true,
      statusAction: StatusAction.ENTER_PASSPHRASE,
    });
    flush();
    assertFalse(isChildVisible(element, '#sync-info'));
    assertFalse(isChildVisible(element, '#sync-paused-info'));
    assertTrue(isChildVisible(element, '#sync-passphrase-error-info'));
    assertFalse(isChildVisible(element, '#sync-other-error-info'));

    // Other sync error.
    webUIListenerCallback('sync-status-changed', {
      signedIn: true,
      hasError: true,
      statusAction: StatusAction.NO_ACTION,
    });
    flush();
    assertFalse(isChildVisible(element, '#sync-info'));
    assertFalse(isChildVisible(element, '#sync-paused-info'));
    assertFalse(isChildVisible(element, '#sync-passphrase-error-info'));
    assertTrue(isChildVisible(element, '#sync-other-error-info'));
  });

  test('ClearBrowsingDataPauseSyncDesktop', function() {
    webUIListenerCallback('sync-status-changed', {
      signedIn: true,
      hasError: false,
    });
    flush();
    assertTrue(!!element.shadowRoot.querySelector(
        '#clearBrowsingDataDialog [slot=footer]'));
    const syncInfo = element.shadowRoot.querySelector('#sync-info');
    assertTrue(isVisible(syncInfo));
    const signoutLink = syncInfo.querySelector('a[href]');
    assertTrue(!!signoutLink);
    assertEquals(0, testSyncBrowserProxy.getCallCount('pauseSync'));
    signoutLink.click();
    assertEquals(1, testSyncBrowserProxy.getCallCount('pauseSync'));
  });

  test('ClearBrowsingDataStartSignInDesktop', function() {
    webUIListenerCallback('sync-status-changed', {
      signedIn: true,
      hasError: true,
      statusAction: StatusAction.REAUTHENTICATE,
    });
    flush();
    assertTrue(!!element.shadowRoot.querySelector(
        '#clearBrowsingDataDialog [slot=footer]'));
    const syncInfo = element.shadowRoot.querySelector('#sync-paused-info');
    assertTrue(isVisible(syncInfo));
    const signinLink = syncInfo.querySelector('a[href]');
    assertTrue(!!signinLink);
    assertEquals(0, testSyncBrowserProxy.getCallCount('startSignIn'));
    signinLink.click();
    assertEquals(1, testSyncBrowserProxy.getCallCount('startSignIn'));
  });

  test('ClearBrowsingDataHandlePassphraseErrorDesktop', function() {
    webUIListenerCallback('sync-status-changed', {
      signedIn: true,
      hasError: true,
      statusAction: StatusAction.ENTER_PASSPHRASE,
    });
    flush();
    assertTrue(!!element.shadowRoot.querySelector(
        '#clearBrowsingDataDialog [slot=footer]'));
    const syncInfo =
        element.shadowRoot.querySelector('#sync-passphrase-error-info');
    assertTrue(isVisible(syncInfo));
    const passphraseLink = syncInfo.querySelector('a[href]');
    assertTrue(!!passphraseLink);
    passphraseLink.click();
    assertEquals(routes.SYNC, Router.getInstance().getCurrentRoute());
  });

  test('ClearBrowsingDataSearchLabelVisibility', function() {
    for (const signedIn of [false, true]) {
      for (const isNonGoogleDse of [false, true]) {
        webUIListenerCallback('update-sync-state', {
          signedIn: signedIn,
          isNonGoogleDse: isNonGoogleDse,
          nonGoogleSearchHistoryString: 'Some test string',
        });
        flush();
        // Test Google search history label visibility and string.
        assertEquals(
            signedIn,
            isVisible(
                element.shadowRoot.querySelector('#googleSearchHistoryLabel')),
            'googleSearchHistoryLabel visibility');
        if (signedIn) {
          expectEquals(
              isNonGoogleDse ?
                  element.i18nAdvanced('clearGoogleSearchHistoryNonGoogleDse') :
                  element.i18nAdvanced('clearGoogleSearchHistoryGoogleDse'),
              element.shadowRoot.querySelector('#googleSearchHistoryLabel')
                  .innerHTML,
              'googleSearchHistoryLabel text');
        }
        // Test non-Google search history label visibility and string.
        assertEquals(
            isNonGoogleDse,
            isVisible(element.shadowRoot.querySelector(
                '#nonGoogleSearchHistoryLabel')),
            'nonGoogleSearchHistoryLabel visibility');
        if (isNonGoogleDse) {
          expectEquals(
              'Some test string',
              element.shadowRoot.querySelector('#nonGoogleSearchHistoryLabel')
                  .innerText,
              'nonGoogleSearchHistoryLabel text');
        }
      }
    }
  });
});

suite('ClearBrowsingDataAllPlatforms', function() {
  /** @type {TestClearBrowsingDataBrowserProxy} */
  let testBrowserProxy;

  /** @type {SettingsClearBrowsingDataDialogElement} */
  let element;

  setup(function() {
    testBrowserProxy = new TestClearBrowsingDataBrowserProxy();
    ClearBrowsingDataBrowserProxyImpl.setInstance(testBrowserProxy);
    PolymerTest.clearBody();
    element = document.createElement('settings-clear-browsing-data-dialog');
    element.set('prefs', getClearBrowsingDataPrefs());
    document.body.appendChild(element);
    return testBrowserProxy.whenCalled('initialize');
  });

  teardown(function() {
    element.remove();
  });

  test('ClearBrowsingDataTap', function() {
    assertTrue(
        element.shadowRoot.querySelector('#clearBrowsingDataDialog').open);
    assertFalse(element.shadowRoot.querySelector('#installedAppsDialog').open);

    const cancelButton = element.shadowRoot.querySelector('.cancel-button');
    assertTrue(!!cancelButton);
    const actionButton = element.shadowRoot.querySelector('.action-button');
    assertTrue(!!actionButton);
    const spinner = element.shadowRoot.querySelector('paper-spinner-lite');
    assertTrue(!!spinner);

    // Select a datatype for deletion to enable the clear button.
    const cookieCheckbox =
        element.shadowRoot.querySelector('#cookiesCheckboxBasic');
    assertTrue(!!cookieCheckbox);
    cookieCheckbox.$.checkbox.click();

    assertFalse(cancelButton.disabled);
    assertFalse(actionButton.disabled);
    assertFalse(spinner.active);

    const promiseResolver = new PromiseResolver();
    testBrowserProxy.setClearBrowsingDataPromise(promiseResolver.promise);
    actionButton.click();

    return testBrowserProxy.whenCalled('clearBrowsingData')
        .then(function(args) {
          const dataTypes = args[0];
          const timePeriod = args[1];
          const installedApps = args[2];
          assertEquals(1, dataTypes.length);
          assertEquals('browser.clear_data.cookies_basic', dataTypes[0]);
          assertTrue(
              element.shadowRoot.querySelector('#clearBrowsingDataDialog')
                  .open);
          assertTrue(cancelButton.disabled);
          assertTrue(actionButton.disabled);
          assertTrue(spinner.active);
          assertTrue(installedApps.length === 0);

          // Simulate signal from browser indicating that clearing has
          // completed.
          webUIListenerCallback('browsing-data-removing', false);
          promiseResolver.resolve(
              {showHistoryNotice: false, showPasswordsNotice: false});
          // Yields to the message loop to allow the callback chain of the
          // Promise that was just resolved to execute before the
          // assertions.
        })
        .then(function() {
          assertFalse(
              element.shadowRoot.querySelector('#clearBrowsingDataDialog')
                  .open);
          assertFalse(cancelButton.disabled);
          assertFalse(actionButton.disabled);
          assertFalse(spinner.active);
          assertFalse(!!element.shadowRoot.querySelector('#historyNotice'));
          assertFalse(!!element.shadowRoot.querySelector('#passwordsNotice'));

          // Check that the dialog didn't switch to installed apps.
          assertFalse(
              element.shadowRoot.querySelector('#installedAppsDialog').open);
        });
  });

  test('ClearBrowsingDataClearButton', function() {
    assertTrue(
        element.shadowRoot.querySelector('#clearBrowsingDataDialog').open);

    const actionButton = element.shadowRoot.querySelector('.action-button');
    assertTrue(!!actionButton);
    const cookieCheckboxBasic =
        element.shadowRoot.querySelector('#cookiesCheckboxBasic');
    assertTrue(!!cookieCheckboxBasic);
    // Initially the button is disabled because all checkboxes are off.
    assertTrue(actionButton.disabled);
    // The button gets enabled if any checkbox is selected.
    cookieCheckboxBasic.$.checkbox.click();
    assertTrue(cookieCheckboxBasic.checked);
    assertFalse(actionButton.disabled);
    // Switching to advanced disables the button.
    element.shadowRoot.querySelector('cr-tabs').selected = 1;
    assertTrue(actionButton.disabled);
    // Switching back enables it again.
    element.shadowRoot.querySelector('cr-tabs').selected = 0;
    assertFalse(actionButton.disabled);
  });

  test('showHistoryDeletionDialog', function() {
    assertTrue(
        element.shadowRoot.querySelector('#clearBrowsingDataDialog').open);
    const actionButton = element.shadowRoot.querySelector('.action-button');
    assertTrue(!!actionButton);

    // Select a datatype for deletion to enable the clear button.
    const cookieCheckbox =
        element.shadowRoot.querySelector('#cookiesCheckboxBasic');
    assertTrue(!!cookieCheckbox);
    cookieCheckbox.$.checkbox.click();
    assertFalse(actionButton.disabled);

    const promiseResolver = new PromiseResolver();
    testBrowserProxy.setClearBrowsingDataPromise(promiseResolver.promise);
    actionButton.click();

    return testBrowserProxy.whenCalled('clearBrowsingData')
        .then(function() {
          // Passing showHistoryNotice = true should trigger the notice about
          // other forms of browsing history to open, and the dialog to stay
          // open.
          promiseResolver.resolve(
              {showHistoryNotice: true, showPasswordsNotice: false});

          // Yields to the message loop to allow the callback chain of the
          // Promise that was just resolved to execute before the
          // assertions.
        })
        .then(function() {
          flush();
          const notice = element.shadowRoot.querySelector('#historyNotice');
          assertTrue(!!notice);
          const noticeActionButton =
              notice.shadowRoot.querySelector('.action-button');
          assertTrue(!!noticeActionButton);

          // The notice should have replaced the main dialog.
          assertFalse(
              element.shadowRoot.querySelector('#clearBrowsingDataDialog')
                  .open);
          assertTrue(notice.shadowRoot.querySelector('#dialog').open);

          const whenNoticeClosed = eventToPromise('close', notice);

          // Tapping the action button will close the notice.
          noticeActionButton.click();

          return whenNoticeClosed;
        })
        .then(function() {
          const notice = element.shadowRoot.querySelector('#historyNotice');
          assertFalse(!!notice);
          assertFalse(
              element.shadowRoot.querySelector('#clearBrowsingDataDialog')
                  .open);
        });
  });

  test('showPasswordsDeletionDialog', function() {
    assertTrue(
        element.shadowRoot.querySelector('#clearBrowsingDataDialog').open);
    const actionButton = element.shadowRoot.querySelector('.action-button');
    assertTrue(!!actionButton);

    // Select a datatype for deletion to enable the clear button.
    const cookieCheckbox =
        element.shadowRoot.querySelector('#cookiesCheckboxBasic');
    assertTrue(!!cookieCheckbox);
    cookieCheckbox.$.checkbox.click();
    assertFalse(actionButton.disabled);

    const promiseResolver = new PromiseResolver();
    testBrowserProxy.setClearBrowsingDataPromise(promiseResolver.promise);
    actionButton.click();

    return testBrowserProxy.whenCalled('clearBrowsingData')
        .then(function() {
          // Passing showPasswordsNotice = true should trigger the notice about
          // incomplete password deletions to open, and the dialog to stay open.
          promiseResolver.resolve(
              {showHistoryNotice: false, showPasswordsNotice: true});

          // Yields to the message loop to allow the callback chain of the
          // Promise that was just resolved to execute before the
          // assertions.
        })
        .then(function() {
          flush();
          const notice = element.shadowRoot.querySelector('#passwordsNotice');
          assertTrue(!!notice);
          const noticeActionButton =
              notice.shadowRoot.querySelector('.action-button');
          assertTrue(!!noticeActionButton);

          // The notice should have replaced the main dialog.
          assertFalse(
              element.shadowRoot.querySelector('#clearBrowsingDataDialog')
                  .open);
          assertTrue(notice.shadowRoot.querySelector('#dialog').open);

          const whenNoticeClosed = eventToPromise('close', notice);

          // Tapping the action button will close the notice.
          noticeActionButton.click();

          return whenNoticeClosed;
        })
        .then(function() {
          const notice = element.shadowRoot.querySelector('#passwordsNotice');
          assertFalse(!!notice);
          assertFalse(
              element.shadowRoot.querySelector('#clearBrowsingDataDialog')
                  .open);
        });
  });

  test('showBothHistoryAndPasswordsDeletionDialog', function() {
    assertTrue(
        element.shadowRoot.querySelector('#clearBrowsingDataDialog').open);
    const actionButton = element.shadowRoot.querySelector('.action-button');
    assertTrue(!!actionButton);

    // Select a datatype for deletion to enable the clear button.
    const cookieCheckbox =
        element.shadowRoot.querySelector('#cookiesCheckboxBasic');
    assertTrue(!!cookieCheckbox);
    cookieCheckbox.$.checkbox.click();
    assertFalse(actionButton.disabled);

    const promiseResolver = new PromiseResolver();
    testBrowserProxy.setClearBrowsingDataPromise(promiseResolver.promise);
    actionButton.click();

    return testBrowserProxy.whenCalled('clearBrowsingData')
        .then(function() {
          // Passing showHistoryNotice = true and showPasswordsNotice = true
          // should first trigger the notice about other forms of browsing
          // history to open, then once that is acknowledged, the notice about
          // incomplete password deletions should open. The main CBD dialog
          // should stay open during that whole time.
          promiseResolver.resolve(
              {showHistoryNotice: true, showPasswordsNotice: true});

          // Yields to the message loop to allow the callback chain of the
          // Promise that was just resolved to execute before the
          // assertions.
        })
        .then(function() {
          flush();
          const notice = element.shadowRoot.querySelector('#historyNotice');
          assertTrue(!!notice);
          const noticeActionButton =
              notice.shadowRoot.querySelector('.action-button');
          assertTrue(!!noticeActionButton);

          // The notice should have replaced the main dialog.
          assertFalse(
              element.shadowRoot.querySelector('#clearBrowsingDataDialog')
                  .open);
          assertTrue(notice.shadowRoot.querySelector('#dialog').open);

          const whenNoticeClosed = eventToPromise('close', notice);

          // Tapping the action button will close the history notice, and
          // display the passwords notice instead.
          noticeActionButton.click();

          return whenNoticeClosed;
        })
        .then(function() {
          // The passwords notice should have replaced the history notice.
          const historyNotice =
              element.shadowRoot.querySelector('#historyNotice');
          assertFalse(!!historyNotice);
          const passwordsNotice =
              element.shadowRoot.querySelector('#passwordsNotice');
          assertTrue(!!passwordsNotice);
        })
        .then(function() {
          flush();
          const notice = element.shadowRoot.querySelector('#passwordsNotice');
          assertTrue(!!notice);
          const noticeActionButton =
              notice.shadowRoot.querySelector('.action-button');
          assertTrue(!!noticeActionButton);

          assertFalse(
              element.shadowRoot.querySelector('#clearBrowsingDataDialog')
                  .open);
          assertTrue(notice.shadowRoot.querySelector('#dialog').open);

          const whenNoticeClosed = eventToPromise('close', notice);

          // Tapping the action button will close the notice.
          noticeActionButton.click();

          return whenNoticeClosed;
        })
        .then(function() {
          const notice = element.shadowRoot.querySelector('#passwordsNotice');
          assertFalse(!!notice);
          assertFalse(
              element.shadowRoot.querySelector('#clearBrowsingDataDialog')
                  .open);
        });
  });

  test('Counters', function() {
    assertTrue(
        element.shadowRoot.querySelector('#clearBrowsingDataDialog').open);

    const checkbox = element.shadowRoot.querySelector('#cacheCheckboxBasic');
    assertEquals('browser.clear_data.cache_basic', checkbox.pref.key);

    // Simulate a browsing data counter result for history. This checkbox's
    // sublabel should be updated.
    webUIListenerCallback('update-counter-text', checkbox.pref.key, 'result');
    assertEquals('result', checkbox.subLabel);
  });

  test('history rows are hidden for supervised users', function() {
    assertFalse(loadTimeData.getBoolean('isSupervised'));
    assertFalse(element.shadowRoot.querySelector('#browsingCheckbox').hidden);
    assertFalse(
        element.shadowRoot.querySelector('#browsingCheckboxBasic').hidden);
    assertFalse(element.shadowRoot.querySelector('#downloadCheckbox').hidden);

    element.remove();
    testBrowserProxy.reset();
    loadTimeData.overrideValues({isSupervised: true});

    element = document.createElement('settings-clear-browsing-data-dialog');
    document.body.appendChild(element);
    flush();

    return testBrowserProxy.whenCalled('initialize').then(function() {
      assertTrue(element.shadowRoot.querySelector('#browsingCheckbox').hidden);
      assertTrue(
          element.shadowRoot.querySelector('#browsingCheckboxBasic').hidden);
      assertTrue(element.shadowRoot.querySelector('#downloadCheckbox').hidden);
    });
  });

  if (isChromeOS) {
    // On ChromeOS the footer is never shown.
    test('ClearBrowsingDataSyncAccountInfo', function() {
      assertTrue(
          element.shadowRoot.querySelector('#clearBrowsingDataDialog').open);

      // Not syncing.
      webUIListenerCallback('sync-status-changed', {
        signedIn: false,
        hasError: false,
      });
      flush();
      assertFalse(!!element.shadowRoot.querySelector(
          '#clearBrowsingDataDialog [slot=footer]'));

      // Syncing.
      webUIListenerCallback('sync-status-changed', {
        signedIn: true,
        hasError: false,
      });
      flush();
      assertFalse(!!element.shadowRoot.querySelector(
          '#clearBrowsingDataDialog [slot=footer]'));

      // Sync passphrase error.
      webUIListenerCallback('sync-status-changed', {
        signedIn: true,
        hasError: true,
        statusAction: StatusAction.ENTER_PASSPHRASE,
      });
      flush();
      assertFalse(!!element.shadowRoot.querySelector(
          '#clearBrowsingDataDialog [slot=footer]'));

      // Other sync error.
      webUIListenerCallback('sync-status-changed', {
        signedIn: true,
        hasError: true,
        statusAction: StatusAction.NO_ACTION,
      });
      flush();
      assertFalse(!!element.shadowRoot.querySelector(
          '#clearBrowsingDataDialog [slot=footer]'));
    });
  }
});

suite('InstalledApps', function() {
  /** @type {TestClearBrowsingDataBrowserProxy} */
  let testBrowserProxy;

  /** @type {SettingsClearBrowsingDataDialogElement} */
  let element;

  /** @type {Array<InstalledApp>} */
  const installedApps = [
    {registerableDomain: 'google.com', isChecked: true},
    {registerableDomain: 'yahoo.com', isChecked: true},
  ];

  setup(() => {
    loadTimeData.overrideValues({installedAppsInCbd: true});
    testBrowserProxy = new TestClearBrowsingDataBrowserProxy();
    testBrowserProxy.setInstalledApps(installedApps);
    ClearBrowsingDataBrowserProxyImpl.setInstance(testBrowserProxy);
    PolymerTest.clearBody();
    element = document.createElement('settings-clear-browsing-data-dialog');
    element.set('prefs', getClearBrowsingDataPrefs());
    document.body.appendChild(element);
    return testBrowserProxy.whenCalled('initialize');
  });

  teardown(() => {
    element.remove();
  });

  test('getInstalledApps', async function() {
    assertTrue(element.$.clearBrowsingDataDialog.open);
    assertFalse(element.$.installedAppsDialog.open);

    // Select cookie checkbox.
    element.$.cookiesCheckboxBasic.$.checkbox.click();
    assertTrue(element.$.cookiesCheckboxBasic.checked);
    // Clear browsing data.
    element.$.clearBrowsingDataConfirm.click();
    assertTrue(element.$.clearBrowsingDataDialog.open);

    await testBrowserProxy.whenCalled('getInstalledApps');
    await whenAttributeIs(element.$.installedAppsDialog, 'open', '');
    const firstInstalledApp =
        element.shadowRoot.querySelector('installed-app-checkbox');
    assertTrue(!!firstInstalledApp);
    assertEquals(
        'google.com', firstInstalledApp.installedApp.registerableDomain);
    assertTrue(firstInstalledApp.installedApp.isChecked);
    // Choose to keep storage for google.com.
    firstInstalledApp.$.checkbox.click();
    assertFalse(firstInstalledApp.installedApp.isChecked);
    // Confirm deletion.
    element.$.installedAppsConfirm.click();
    const [dataTypes, timePeriod, apps] =
        await testBrowserProxy.whenCalled('clearBrowsingData');
    assertEquals(1, dataTypes.length);
    assertEquals('browser.clear_data.cookies_basic', dataTypes[0]);
    assertEquals(2, apps.length);
    assertEquals('google.com', apps[0].registerableDomain);
    assertFalse(apps[0].isChecked);
    assertEquals('yahoo.com', apps[1].registerableDomain);
    assertTrue(apps[1].isChecked);
  });
});
