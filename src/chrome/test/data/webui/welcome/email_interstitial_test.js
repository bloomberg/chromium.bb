// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('onboarding_welcome_email_interstitial', function() {
  suite('EmailInterstitialTest', function() {
    const emails = [
      {
        id: 0,
        name: 'Gmail',
        icon: 'gmail',
        url: 'http://www.gmail.com',
      },
      {
        id: 1,
        name: 'Yahoo',
        icon: 'yahoo',
        url: 'http://mail.yahoo.com',
      }
    ];

    /** @type {nux.NuxEmailProxy} */
    let testEmailBrowserProxy;

    /** @type {nux.EmailInterstitialProxy} */
    let testEmailInterstitialProxy;

    /** @type {welcome.WelcomeBrowserProxy} */
    let testWelcomeBrowserProxy;

    /** @type {EmailChooserElement} */
    let testElement;

    setup(function() {
      testEmailBrowserProxy = new TestNuxEmailProxy();
      testEmailBrowserProxy.setEmailList(emails);
      nux.EmailAppProxyImpl.instance_ = testEmailBrowserProxy;

      testEmailInterstitialProxy = new TestEmailInterstititalProxy();
      nux.EmailInterstitialProxyImpl.instance_ = testEmailInterstitialProxy;

      testWelcomeBrowserProxy = new TestWelcomeBrowserProxy();
      welcome.WelcomeBrowserProxyImpl.instance_ = testWelcomeBrowserProxy;

      PolymerTest.clearBody();
      testElement = document.createElement('email-interstitial');
      document.body.appendChild(testElement);
    });

    teardown(function() {
      testElement.remove();
    });

    test('email interstitial click continue button twice', function() {
      // 'Continue' button is only paper-button.
      const continueButton = testElement.$$('paper-button');
      assertTrue(!!continueButton);

      continueButton.click();
      continueButton.click();

      assertEquals(1, testEmailBrowserProxy.getCallCount('getAppList'));
      assertEquals(1, testEmailInterstitialProxy.getCallCount('recordNext'));
      return testWelcomeBrowserProxy.whenCalled('goToURL').then(url => {
        assertEquals(url, emails[0].url);
      });
    });
  });
});
