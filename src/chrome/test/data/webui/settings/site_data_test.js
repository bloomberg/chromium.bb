// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {LocalDataBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {MetricsBrowserProxyImpl, PrivacyElementInteractions, Router,routes} from 'chrome://settings/settings.js';
import {TestLocalDataBrowserProxy} from 'chrome://test/settings/test_local_data_browser_proxy.js';
import {eventToPromise} from 'chrome://test/test_util.m.js';

import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

// clang-format on

suite('SiteDataTest', function() {
  /** @type {SiteDataElement} */
  let siteData;

  /** @type {TestLocalDataBrowserProxy} */
  let testBrowserProxy;

  /** @type {!TestMetricsBrowserProxy} */
  let testMetricsBrowserProxy;

  setup(function() {
    Router.getInstance().navigateTo(routes.SITE_SETTINGS);
    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.instance_ = testMetricsBrowserProxy;
    testBrowserProxy = new TestLocalDataBrowserProxy();
    LocalDataBrowserProxyImpl.instance_ = testBrowserProxy;
    siteData = document.createElement('site-data');
    siteData.filter = '';
  });

  teardown(function() {
    siteData.remove();
  });

  test('remove button (trash) calls remove on origin', function() {
    const promise =
        eventToPromise('site-data-list-complete', siteData)
            .then(() => {
              flush();
              const button =
                  siteData.$$('site-data-entry').$$('.icon-delete-gray');
              assertTrue(!!button);
              assertEquals('CR-ICON-BUTTON', button.tagName);
              button.click();
              return testBrowserProxy.whenCalled('removeItem');
            })
            .then(function(path) {
              assertEquals('Hello', path);
              return testMetricsBrowserProxy.whenCalled(
                  'recordSettingsPageHistogram');
            })
            .then(metric => {
              assertEquals(
                  PrivacyElementInteractions.SITE_DATA_REMOVE_SITE, metric);
            });
    const sites = [
      {site: 'Hello', localData: 'Cookiez!'},
    ];
    testBrowserProxy.setCookieList(sites);
    document.body.appendChild(siteData);
    Router.getInstance().navigateTo(routes.SITE_SETTINGS_SITE_DATA);
    return promise;
  });

  test('remove button hidden when no search results', function() {
    const promise = eventToPromise('site-data-list-complete', siteData)
                        .then(() => {
                          assertEquals(2, siteData.$.list.items.length);
                          const promise2 = eventToPromise(
                              'site-data-list-complete', siteData);
                          siteData.filter = 'Hello';
                          return promise2;
                        })
                        .then(() => {
                          assertEquals(1, siteData.$.list.items.length);
                        });
    const sites = [
      {site: 'Hello', localData: 'Cookiez!'},
      {site: 'World', localData: 'Cookiez!'},
    ];
    testBrowserProxy.setCookieList(sites);
    document.body.appendChild(siteData);
    Router.getInstance().navigateTo(routes.SITE_SETTINGS_SITE_DATA);
    return promise;
  });

  test('calls reloadCookies() when created', function() {
    Router.getInstance().navigateTo(routes.SITE_SETTINGS_SITE_DATA);
    document.body.appendChild(siteData);
    Router.getInstance().navigateTo(routes.COOKIES);
    return testBrowserProxy.whenCalled('reloadCookies');
  });

  test('calls reloadCookies() when visited again', function() {
    document.body.appendChild(siteData);
    Router.getInstance().navigateTo(routes.COOKIES);
    testBrowserProxy.reset();
    Router.getInstance().navigateTo(routes.SITE_SETTINGS_SITE_DATA);
    return testBrowserProxy.whenCalled('reloadCookies');
  });

  test('calls getDisplayList() when search param present', async function() {
    // Check that providing a search query parameter while navigating and a
    // search filter to the cookies page does not reload cookies, but instead
    // updates the list.
    document.body.appendChild(siteData);
    Router.getInstance().navigateTo(routes.BASIC);
    const params = new URLSearchParams();
    params.append('searchSubpage', 'test');
    Router.getInstance().navigateTo(routes.SITE_SETTINGS_SITE_DATA, params);
    siteData.filter = 'test';
    const filter = await testBrowserProxy.whenCalled('getDisplayList');
    assertEquals('test', filter);
    assertEquals(0, testBrowserProxy.getCallCount('reloadCookies'));
  });

  test('remove button records interaction metric', async function() {
    // Check that the remove button correctly records an interaction metric
    // based on whether the list is filtered or not.
    document.body.appendChild(siteData);
    siteData.$$('#removeShowingSites').click();
    flush();

    siteData.$$('.action-button').click();
    let metric =
        await testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram');
    assertEquals(PrivacyElementInteractions.SITE_DATA_REMOVE_ALL, metric);
    testMetricsBrowserProxy.reset();

    // Add a filter and repeat.
    siteData.filter = 'Test';
    siteData.$$('#removeShowingSites').click();
    flush();

    siteData.$$('.action-button').click();
    metric =
        await testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram');
    assertEquals(PrivacyElementInteractions.SITE_DATA_REMOVE_FILTERED, metric);
  });
});
