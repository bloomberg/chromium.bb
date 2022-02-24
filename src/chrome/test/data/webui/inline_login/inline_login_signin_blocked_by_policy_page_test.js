// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests for the Inline login flow ('Add account' flow) on
 * Chrome OS when a secondary account is being added and the sign-in should be
 * blocked by the policy SecondaryGoogleAccountUsage.
 */

import 'chrome://chrome-signin/inline_login_app.js';

import {InlineLoginBrowserProxyImpl} from 'chrome://chrome-signin/inline_login_browser_proxy.js';
import {AccountAdditionOptions} from 'chrome://chrome-signin/inline_login_util.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertEquals, assertFalse, assertTrue} from '../chai_assert.js';

import {fakeSigninBlockedByPolicyData, TestAuthenticator, TestInlineLoginBrowserProxy} from './inline_login_test_util.js';

window.inline_login_signin_blocked_by_policy_page_test = {};
const inline_login_signin_blocked_by_policy_page_test =
    window.inline_login_signin_blocked_by_policy_page_test;
inline_login_signin_blocked_by_policy_page_test.suiteName =
    'InlineLoginSigninBlockedByPolicyPageTest';

/** @enum {string} */
inline_login_signin_blocked_by_policy_page_test.TestNames = {
  BlockedSigninPage: 'BlockedSigninPage',
  FireWebUIListenerCallback: 'FireWebUIListenerCallback',
  OkButton: 'OkButton',
};

suite(inline_login_signin_blocked_by_policy_page_test.suiteName, () => {
  /** @type {SigninBlockedByPolicyPageElement} */
  let signinBlockedByPolicyPageComponent;
  /** @type {InlineLoginAppElement} */
  let inlineLoginComponent;
  /** @type {TestInlineLoginBrowserProxy} */
  let testBrowserProxy;
  /** @type {TestAuthenticator} */
  let testAuthenticator;

  /**
   * @return {string} id of the active view.
   */
  function getActiveViewId() {
    return inlineLoginComponent.$$('div.active[slot="view"]').id;
  }

  /**
   * @param {?AccountAdditionOptions} dialogArgs
   */
  function testSetup(dialogArgs) {
    document.body.innerHTML = '';
    testBrowserProxy = new TestInlineLoginBrowserProxy();
    testBrowserProxy.setDialogArguments(dialogArgs);
    InlineLoginBrowserProxyImpl.instance_ = testBrowserProxy;
    document.body.innerHTML = '';
    inlineLoginComponent = /** @type {InlineLoginAppElement} */ (
        document.createElement('inline-login-app'));
    document.body.appendChild(inlineLoginComponent);
    testAuthenticator = new TestAuthenticator();
    inlineLoginComponent.setAuthExtHostForTest(testAuthenticator);
    flush();
    signinBlockedByPolicyPageComponent =
        inlineLoginComponent.$$('signin-blocked-by-policy-page');
  }

  test(
      assert(inline_login_signin_blocked_by_policy_page_test.TestNames
                 .BlockedSigninPage),
      () => {
        testSetup(/*dialogArgs=*/ null);
        // Fire web UI listener to switch the ui view to
        // `signinBlockedByPolicy`.
        webUIListenerCallback(
            'show-signin-blocked-by-policy-page',
            fakeSigninBlockedByPolicyData);
        assertEquals(
            inlineLoginComponent.View.signinBlockedByPolicy, getActiveViewId(),
            'Sing-in blocked by policy page should be shown');

        const title =
            signinBlockedByPolicyPageComponent.shadowRoot.querySelector('h1');
        assertEquals(
            title.textContent, 'Sign-in blocked by your admin',
            'The titles do not match');
        const textBody =
            signinBlockedByPolicyPageComponent.shadowRoot.querySelector(
                '.secondary');
        assertEquals(
            textBody.textContent,
            'john.doe@example.com is managed by example.com and your admin has blocked sign-in as a secondary account',
            'The text bodies do not match');
      });

  test(
      assert(inline_login_signin_blocked_by_policy_page_test.TestNames
                 .FireWebUIListenerCallback),
      () => {
        testSetup(/*dialogArgs=*/ null);
        // Fire web UI listener to switch the ui view to
        // `signinBlockedByPolicy`.
        webUIListenerCallback(
            'show-signin-blocked-by-policy-page',
            fakeSigninBlockedByPolicyData);
        assertEquals(
            inlineLoginComponent.View.signinBlockedByPolicy, getActiveViewId(),
            'Sing-in blocked by policy should be shown');
        let textBody =
            signinBlockedByPolicyPageComponent.shadowRoot.querySelector(
                '.secondary');
        assertTrue(
            textBody.textContent.includes('john.doe@example.com'),
            'Invalid user email');
        assertTrue(
            textBody.textContent.includes('example.com'),
            'Invalid hosted domain');
        webUIListenerCallback(
            'show-signin-blocked-by-policy-page',
            {email: 'coyote@acme.com', hostedDomain: 'acme.com'});
        textBody = signinBlockedByPolicyPageComponent.shadowRoot.querySelector(
            '.secondary');
        assertTrue(
            textBody.textContent.includes('coyote@acme.com'),
            'Invalid user email');
        assertTrue(
            textBody.textContent.includes('acme.com'), 'Invalid hosted domain');
      });

  test(
      assert(
          inline_login_signin_blocked_by_policy_page_test.TestNames.OkButton),
      async () => {
        testSetup(/*dialogArgs=*/ null);
        // Fire web UI listener to switch the ui view to
        // `signinBlockedByPolicy`.
        webUIListenerCallback(
            'show-signin-blocked-by-policy-page',
            fakeSigninBlockedByPolicyData);

        const okButton = inlineLoginComponent.$$('.next-button');
        // OK button and signin blocked by policy screen should be visible.
        assertFalse(okButton.hidden, 'OK button should be visible');
        assertEquals(
            inlineLoginComponent.View.signinBlockedByPolicy, getActiveViewId(),
            'Sing-in blocked by policy should be shown');

        okButton.click();
        await testBrowserProxy.whenCalled('dialogClose');
      });
});
