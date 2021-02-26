// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://signin-reauth/signin_reauth_app.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {SigninReauthBrowserProxyImpl} from 'chrome://signin-reauth/signin_reauth_browser_proxy.js';
import {isVisible} from 'chrome://test/test_util.m.js';

import {TestSigninReauthBrowserProxy} from './test_signin_reauth_browser_proxy.js';

suite('SigninReauthTest', function() {
  let app;

  /** @type {TestSigninReauthBrowserProxy} */
  let browserProxy;

  setup(function() {
    browserProxy = new TestSigninReauthBrowserProxy();
    SigninReauthBrowserProxyImpl.instance_ = browserProxy;
    PolymerTest.clearBody();
    app = document.createElement('signin-reauth-app');
    document.body.append(app);
  });

  function assertDefaultLocale() {
    // This test makes comparisons with strings in their default locale,
    // which is en-US.
    assertEquals(
        'en-US', navigator.language,
        'Cannot verify strings for the ' + navigator.language + 'locale.');
  }

  // Tests that no DCHECKS are thrown during initialization of the UI.
  test('LoadPage', function() {
    assertDefaultLocale();
    assertEquals(
        'Use your Google Account to save and fill passwords?',
        app.$.signinReauthTitle.textContent.trim());
  });

  test('ClickConfirm', function() {
    app.$.confirmButton.click();
    return browserProxy.whenCalled('confirm');
  });

  test('ClickCancel', function() {
    app.$.cancelButton.click();
    return browserProxy.whenCalled('cancel');
  });

  const requires_reauth_test_params = [
    {
      requires_reauth: true,
    },
    {
      requires_reauth: false,
    },
  ];

  requires_reauth_test_params.forEach(function(params) {
    test('ButtonsVisibilityAndFocus', async () => {
      await browserProxy.whenCalled('initialize');
      assertFalse(isVisible(app.$.confirmButton));
      assertFalse(isVisible(app.$.cancelButton));
      assertTrue(isVisible(app.$$('paper-spinner-lite')));

      webUIListenerCallback('reauth-type-received', params.requires_reauth);
      flush();

      assertTrue(isVisible(app.$.confirmButton));
      assertTrue(isVisible(app.$.cancelButton));
      assertFalse(isVisible(app.$$('paper-spinner-lite')));

      assertEquals(getDeepActiveElement(), app.$.confirmButton);

      assertDefaultLocale();
      assertEquals('Yes', app.$.confirmButton.textContent.trim());
    });
  });
});
