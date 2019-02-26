// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'landing-view',

  behaviors: [welcome.NavigationBehavior],

  /** @private {?nux.LandingViewProxy} */
  landingViewProxy_: null,

  /** @private {boolean} */
  finalized_: false,

  /** @override */
  ready() {
    this.landingViewProxy_ = nux.LandingViewProxyImpl.getInstance();
  },

  onRouteEnter: function() {
    this.finalized_ = false;
    this.landingViewProxy_.recordPageShown();
  },

  onRouteUnload: function() {
    // Clicking on 'Returning user' will change the URL.
    if (this.finalized_) {
      return;
    }
    this.finalized_ = true;
    this.landingViewProxy_.recordNavigatedAway();
  },

  /** @private */
  onExistingUserClick_: function() {
    this.finalized_ = true;
    this.landingViewProxy_.recordExistingUser();
    welcome.WelcomeBrowserProxyImpl.getInstance().handleActivateSignIn(
        'chrome://welcome/returning-user');
  },

  /** @private */
  onNewUserClick_: function() {
    this.finalized_ = true;
    this.landingViewProxy_.recordNewUser();
    welcome.navigateTo(welcome.Routes.NEW_USER, 1);
  }
});
