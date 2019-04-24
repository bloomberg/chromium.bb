// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'signin-view',

  behaviors: [welcome.NavigationBehavior],

  /** @private {boolean} */
  shouldShowEmailInterstitial_:
      loadTimeData.getBoolean('showEmailInterstitial'),

  /** @private {boolean} */
  finalized_: false,

  /** @private {?welcome.WelcomeBrowserProxy} */
  welcomeBrowserProxy_: null,

  /** @private {?nux.SigninViewProxy} */
  signinViewProxy_: null,

  /** @override */
  ready: function() {
    this.welcomeBrowserProxy_ = welcome.WelcomeBrowserProxyImpl.getInstance();
    this.signinViewProxy_ = nux.SigninViewProxyImpl.getInstance();
  },

  onRouteEnter: function() {
    this.finalized_ = false;
    this.signinViewProxy_.recordPageShown();
  },

  onRouteExit: function() {
    if (this.finalized_) {
      return;
    }
    this.finalized_ = true;
    this.signinViewProxy_.recordNavigatedAwayThroughBrowserHistory();
  },

  onRouteUnload: function() {
    // URL is expected to change when signing in or skipping.
    if (this.finalized_) {
      return;
    }
    this.finalized_ = true;
    this.signinViewProxy_.recordNavigatedAway();
  },

  /**
   * @return {?string}
   * @private
   */
  getTargetUrl_: function() {
    const savedProvider =
        nux.EmailAppProxyImpl.getInstance().getSavedProvider();
    if (savedProvider != undefined && this.shouldShowEmailInterstitial_) {
      return `chrome://welcome/email-interstitial?provider=${savedProvider}`;
    } else {
      return null;
    }
  },

  /**
   * When the user clicks sign-in, check whether or not they previously
   * selected an email provider they prefer to use. If so, direct them back to
   * the email-interstitial page, otherwise let it direct to NTP.
   * @private
   */
  onSignInClick_: function() {
    this.finalized_ = true;
    this.signinViewProxy_.recordSignIn();
    this.welcomeBrowserProxy_.handleActivateSignIn(this.getTargetUrl_());
  },

  /** @private */
  onNoThanksClick_: function() {
    this.finalized_ = true;
    this.signinViewProxy_.recordSkip();
    // It's safe to assume sign-view is always going to be the last step, so
    // go to the target url directly. If there's no target, it lands on NTP.
    this.welcomeBrowserProxy_.handleUserDecline(this.getTargetUrl_());
  }
});
