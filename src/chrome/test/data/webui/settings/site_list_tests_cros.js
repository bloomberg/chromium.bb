// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {AndroidInfoBrowserProxyImpl,ContentSetting,ContentSettingsTypes,SiteSettingsPrefsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {Router} from 'chrome://settings/settings.js';
import {TEST_ANDROID_SMS_ORIGIN, TestAndroidInfoBrowserProxy} from 'chrome://test/settings/test_android_info_browser_proxy.js';
import {TestSiteSettingsPrefsBrowserProxy} from 'chrome://test/settings/test_site_settings_prefs_browser_proxy.js';
import {createContentSettingTypeToValuePair,createRawSiteException,createSiteSettingsPrefs} from 'chrome://test/settings/test_util.js';

// clang-format on

suite('SiteListChromeOS', function() {
  /**
   * A site list element created before each test.
   * @type {SiteList}
   */
  let testElement;

  /**
   * The mock proxy object to use during test.
   * @type {TestSiteSettingsPrefsBrowserProxy}
   */
  let browserProxy = null;

  /**
   * Mock AndroidInfoBrowserProxy to use during test.
   * @type {TestAndroidInfoBrowserProxy}
   */
  let androidInfoBrowserProxy = null;

  /**
   * An example Javascript pref for android_sms notification setting.
   * @type {SiteSettingsPref}
   */
  let prefsAndroidSms;

  // Initialize a site-list before each test.
  setup(function() {
    prefsAndroidSms = createSiteSettingsPrefs(
        [], [createContentSettingTypeToValuePair(
                ContentSettingsTypes.NOTIFICATIONS, [
                  // android sms setting.
                  createRawSiteException(TEST_ANDROID_SMS_ORIGIN),
                  // Non android sms setting that should be handled as usual.
                  createRawSiteException('http://bar.com')
                ])]);

    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.instance_ = browserProxy;
    androidInfoBrowserProxy = new TestAndroidInfoBrowserProxy();
    AndroidInfoBrowserProxyImpl.instance_ = androidInfoBrowserProxy;

    PolymerTest.clearBody();
    testElement = document.createElement('site-list');
    testElement.searchFilter = '';
    document.body.appendChild(testElement);
  });

  teardown(function() {
    // The code being tested changes the Route. Reset so that state is not
    // leaked across tests.
    Router.getInstance().resetRouteForTesting();

    // Reset multidevice enabled flag.
    loadTimeData.overrideValues({multideviceAllowedByPolicy: false});
  });

  /** Configures the test element. */
  function setUpAndroidSmsNotifications() {
    browserProxy.setPrefs(prefsAndroidSms);
    testElement.categorySubtype = ContentSetting.ALLOW;
    // Some route is needed, but the actual route doesn't matter.
    testElement.currentRoute = {
      page: 'dummy',
      section: 'privacy',
      subpage: ['site-settings', 'site-settings-category-location'],
    };
    testElement.category = ContentSettingsTypes.NOTIFICATIONS;
  }

  test('update androidSmsInfo', function() {
    setUpAndroidSmsNotifications();
    assertEquals(0, androidInfoBrowserProxy.getCallCount('getAndroidSmsInfo'));

    loadTimeData.overrideValues({multideviceAllowedByPolicy: true});
    setUpAndroidSmsNotifications();
    // Assert 2 calls since the observer observes 2 properties.
    assertEquals(2, androidInfoBrowserProxy.getCallCount('getAndroidSmsInfo'));

    return Promise
        .all([
          androidInfoBrowserProxy.whenCalled('getAndroidSmsInfo'),
          browserProxy.whenCalled('getExceptionList'),
        ])
        .then(results => {
          const contentType = results[1];
          flush();
          assertEquals(ContentSettingsTypes.NOTIFICATIONS, contentType);
          assertEquals(2, testElement.sites.length);

          assertEquals(
              prefsAndroidSms.exceptions[contentType][0].origin,
              testElement.sites[0].origin);
          assertTrue(testElement.sites[0].showAndroidSmsNote);

          assertEquals(
              prefsAndroidSms.exceptions[contentType][1].origin,
              testElement.sites[1].origin);
          assertEquals(undefined, testElement.sites[1].showAndroidSmsNote);
        });
  });
});
