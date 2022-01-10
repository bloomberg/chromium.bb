// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {DefaultBrowserBrowserProxy, DefaultBrowserBrowserProxyImpl, DefaultBrowserInfo, SettingsDefaultBrowserPageElement} from 'chrome://settings/settings.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
// clang-format on

/**
 * A test version of DefaultBrowserBrowserProxy. Provides helper methods
 * for allowing tests to know when a method was called, as well as
 * specifying mock responses.
 */
class TestDefaultBrowserBrowserProxy extends TestBrowserProxy implements
    DefaultBrowserBrowserProxy {
  private defaultBrowserInfo_: DefaultBrowserInfo;

  constructor() {
    super([
      'requestDefaultBrowserState',
      'setAsDefaultBrowser',
    ]);

    this.defaultBrowserInfo_ = {
      canBeDefault: true,
      isDefault: false,
      isDisabledByPolicy: false,
      isUnknownError: false
    };
  }

  requestDefaultBrowserState() {
    this.methodCalled('requestDefaultBrowserState');
    return Promise.resolve(this.defaultBrowserInfo_);
  }

  setAsDefaultBrowser() {
    this.methodCalled('setAsDefaultBrowser');
  }

  /**
   * Sets the response to be returned by |requestDefaultBrowserState|.
   * @param info Fake info for testing.
   */
  setDefaultBrowserInfo(info: DefaultBrowserInfo) {
    this.defaultBrowserInfo_ = info;
  }
}

suite('DefaultBrowserPageTest', function() {
  let page: SettingsDefaultBrowserPageElement;
  let browserProxy: TestDefaultBrowserBrowserProxy;

  setup(function() {
    browserProxy = new TestDefaultBrowserBrowserProxy();
    DefaultBrowserBrowserProxyImpl.setInstance(browserProxy);
    return initPage();
  });

  teardown(function() {
    page.remove();
  });

  function initPage() {
    browserProxy.reset();
    document.body.innerHTML = '';
    page = document.createElement('settings-default-browser-page');
    document.body.appendChild(page);
    return browserProxy.whenCalled('requestDefaultBrowserState');
  }

  test('default-browser-test-can-be-default', function() {
    browserProxy.setDefaultBrowserInfo({
      canBeDefault: true,
      isDefault: false,
      isDisabledByPolicy: false,
      isUnknownError: false
    });

    return initPage().then(function() {
      flush();
      assertTrue(!!page.shadowRoot!.querySelector<HTMLElement>(
          '#canBeDefaultBrowser'));
      assertTrue(!page.shadowRoot!.querySelector<HTMLElement>('#isDefault'));
      assertTrue(
          !page.shadowRoot!.querySelector<HTMLElement>('#isSecondaryInstall'));
      assertTrue(
          !page.shadowRoot!.querySelector<HTMLElement>('#isUnknownError'));
    });
  });

  test('default-browser-test-is-default', function() {
    assertTrue(!!page);
    browserProxy.setDefaultBrowserInfo({
      canBeDefault: true,
      isDefault: true,
      isDisabledByPolicy: false,
      isUnknownError: false
    });

    return initPage().then(function() {
      flush();
      assertFalse(!!page.shadowRoot!.querySelector<HTMLElement>(
          '#canBeDefaultBrowser'));
      assertFalse(
          page.shadowRoot!.querySelector<HTMLElement>('#isDefault')!.hidden);
      assertTrue(
          page.shadowRoot!.querySelector<HTMLElement>(
                              '#isSecondaryInstall')!.hidden);
      assertTrue(page.shadowRoot!.querySelector<HTMLElement>(
                                     '#isUnknownError')!.hidden);
    });
  });

  test('default-browser-test-is-secondary-install', function() {
    browserProxy.setDefaultBrowserInfo({
      canBeDefault: false,
      isDefault: false,
      isDisabledByPolicy: false,
      isUnknownError: false
    });

    return initPage().then(function() {
      flush();
      assertFalse(!!page.shadowRoot!.querySelector<HTMLElement>(
          '#canBeDefaultBrowser'));
      assertTrue(
          page.shadowRoot!.querySelector<HTMLElement>('#isDefault')!.hidden);
      assertFalse(
          page.shadowRoot!.querySelector<HTMLElement>(
                              '#isSecondaryInstall')!.hidden);
      assertTrue(page.shadowRoot!.querySelector<HTMLElement>(
                                     '#isUnknownError')!.hidden);
    });
  });

  test('default-browser-test-is-disabled-by-policy', function() {
    browserProxy.setDefaultBrowserInfo({
      canBeDefault: true,
      isDefault: false,
      isDisabledByPolicy: true,
      isUnknownError: false
    });

    return initPage().then(function() {
      flush();
      assertFalse(!!page.shadowRoot!.querySelector<HTMLElement>(
          '#canBeDefaultBrowser'));
      assertTrue(
          page.shadowRoot!.querySelector<HTMLElement>('#isDefault')!.hidden);
      assertTrue(
          page.shadowRoot!.querySelector<HTMLElement>(
                              '#isSecondaryInstall')!.hidden);
      assertFalse(
          page.shadowRoot!.querySelector<HTMLElement>(
                              '#isUnknownError')!.hidden);
    });
  });

  test('default-browser-test-is-unknown-error', function() {
    browserProxy.setDefaultBrowserInfo({
      canBeDefault: true,
      isDefault: false,
      isDisabledByPolicy: false,
      isUnknownError: true
    });

    return initPage().then(function() {
      flush();
      assertFalse(!!page.shadowRoot!.querySelector<HTMLElement>(
          '#canBeDefaultBrowser'));
      assertTrue(
          page.shadowRoot!.querySelector<HTMLElement>('#isDefault')!.hidden);
      assertTrue(
          page.shadowRoot!.querySelector<HTMLElement>(
                              '#isSecondaryInstall')!.hidden);
      assertFalse(
          page.shadowRoot!.querySelector<HTMLElement>(
                              '#isUnknownError')!.hidden);
    });
  });
});
