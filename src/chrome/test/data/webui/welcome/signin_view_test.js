// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://welcome/signin_view.js';

import {SigninViewProxyImpl} from 'chrome://welcome/signin_view_proxy.js';
import {WelcomeBrowserProxyImpl} from 'chrome://welcome/welcome_browser_proxy.js';

import {TestSigninViewProxy} from './test_signin_view_proxy.js';
import {TestWelcomeBrowserProxy} from './test_welcome_browser_proxy.js';

suite('SigninViewTest', function() {
  /** @type {SigninViewElement} */
  let testElement;

  /** @type {WelcomeBrowserProxy} */
  let testWelcomeBrowserProxy;

  setup(function() {
    testWelcomeBrowserProxy = new TestWelcomeBrowserProxy();
    WelcomeBrowserProxyImpl.setInstance(testWelcomeBrowserProxy);

    // Not used in test, but setting to test proxy anyway, in order to prevent
    // calls to backend.
    SigninViewProxyImpl.setInstance(new TestSigninViewProxy());

    document.body.innerHTML = '';
    testElement = document.createElement('signin-view');
    document.body.appendChild(testElement);
  });

  teardown(function() {
    testElement.remove();
  });

  test('sign-in button', function() {
    const signinButton = testElement.shadowRoot.querySelector('cr-button');
    assertTrue(!!signinButton);

    signinButton.click();
    return testWelcomeBrowserProxy.whenCalled('handleActivateSignIn')
        .then(redirectUrl => assertEquals(null, redirectUrl));
  });

  test('no-thanks button', function() {
    const noThanksButton = testElement.shadowRoot.querySelector('button');
    assertTrue(!!noThanksButton);
    noThanksButton.click();
    return testWelcomeBrowserProxy.whenCalled('handleUserDecline');
  });
});
