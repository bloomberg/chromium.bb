// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Supervision Onboarding polymer element. It loads onboarding
 * pages from the web in a webview and forwards user actions to the
 * OnboardingController, which contains the full flow state machine.
 */

{
  class WebviewLoader {
    /** @param {!WebView} webview */
    constructor(webview) {
      this.webview_ = webview;

      /**
       * Page being currently loaded. If null we are not loading any page yet.
       * @private {?chromeos.supervision.mojom.OnboardingPage}
       */
      this.page_ = null;

      /**
       * Custom header values found in responses to requests made by the
       * webview.
       * @private {?string}
       */
      this.customHeaderValue_ = null;

      /**
       * Pending callback for a webview page load. It will be called with the
       * list of custom header values if asked by the controller, or an empty
       * array otherwise.
       * @private {?function({customHeaderValue: ?string})}
       */
      this.pendingLoadPageCallback_ = null;

      this.webviewListener_ = this.webviewFinishedLoading_.bind(this);
      this.webviewHeaderListener_ = this.onHeadersReceived_.bind(this);
      this.webviewAuthListener_ = this.authorizeRequest_.bind(this);
    }

    /**
     * @param {!chromeos.supervision.mojom.OnboardingPage} page
     * @return {!Promise<{customHeaderValue: ?string}>}
     */
    loadPage(page) {
      // TODO(958995): Handle the case where we are still loading the previous
      // page but the controller wants to load the next one. For now we just
      // resolve the previous callback.
      if (this.pendingLoadPageCallback_) {
        this.pendingLoadPageCallback_({customHeaderValue: null});
      }

      this.page_ = page;
      this.customHeaderValue_ = null;
      this.pendingLoadPageCallback_ = null;

      this.webview_.request.onBeforeSendHeaders.addListener(
          this.webviewAuthListener_,
          {urls: [page.urlFilterPattern], types: ['main_frame']},
          ['blocking', 'requestHeaders']);
      this.webview_.request.onHeadersReceived.addListener(
          this.webviewHeaderListener_,
          {urls: [page.urlFilterPattern], types: ['main_frame']},
          ['responseHeaders', 'extraHeaders']);

      // TODO(958995): Report load errors through the mojo interface.
      // At the moment we are treating any loadstop/abort event as a successful
      // load.
      this.webview_.addEventListener('loadstop', this.webviewListener_);
      this.webview_.addEventListener('loadabort', this.webviewListener_);
      this.webview_.src = page.url.url;

      return new Promise(resolve => {
        this.pendingLoadPageCallback_ = resolve;
      });
    }

    /**
     * @param {!Object<{responseHeaders:
     *     !Array<{name: string, value:string}>}>} responseEvent
     * @return {!BlockingResponse}
     * @private
     */
    onHeadersReceived_(responseEvent) {
      if (!this.page_.customHeaderName) {
        return {};
      }

      const header = responseEvent.responseHeaders.find(
          h => h.name.toUpperCase() ==
              this.page_.customHeaderName.toUpperCase());
      this.customHeaderValue_ = header ? header.value : null;

      return {};
    }

    /**
     * Injects headers into the passed request.
     *
     * @param {!Object} requestEvent
     * @return {!BlockingResponse} Modified headers.
     * @private
     */
    authorizeRequest_(requestEvent) {
      requestEvent.requestHeaders.push(
          {name: 'Authorization', value: 'Bearer ' + this.page_.accessToken});

      return /** @type {!BlockingResponse} */ ({
        requestHeaders: requestEvent.requestHeaders,
      });
    }

    /**
     * Called when the webview sends a loadstop or loadabort event.
     * @private {!Event} e
     */
    webviewFinishedLoading_(e) {
      this.webview_.request.onBeforeSendHeaders.removeListener(
          this.webviewAuthListener_);
      this.webview_.request.onHeadersReceived.removeListener(
          this.webviewHeaderListener_);
      this.webview_.removeEventListener('loadstop', this.webviewListener_);
      this.webview_.removeEventListener('loadabort', this.webviewListener_);

      this.pendingLoadPageCallback_({
        customHeaderValue: this.customHeaderValue_,
      });
    }
  }

  Polymer({
    is: 'supervision-onboarding',

    behaviors: [LoginScreenBehavior, OobeDialogHostBehavior],

    properties: {
      /** True if the webview loaded the page. */
      pageLoaded_: {type: Boolean, value: false},
    },

    /** Overridden from LoginScreenBehavior. */
    EXTERNAL_API: [
      'setupMojo',
    ],

    /** @private {?chromeos.supervision.mojom.OnboardingControllerProxy} */
    controller_: null,

    /**
     * @private {?chromeos.supervision.mojom.
     *     OnboardingWebviewHostCallbackRouter}
     */
    hostCallbackRouter_: null,

    /** @private {?WebviewLoader} */
    webviewLoader_: null,

    setupMojo: function() {
      this.webviewLoader_ =
          new WebviewLoader(this.$.supervisionOnboardingWebview);

      this.controller_ =
          chromeos.supervision.mojom.OnboardingController.getProxy();

      this.hostCallbackRouter_ =
          new chromeos.supervision.mojom.OnboardingWebviewHostCallbackRouter();

      this.hostCallbackRouter_.loadPage.addListener(p => {
        this.pageLoaded_ = false;
        return this.webviewLoader_.loadPage(p).then(response => {
          this.pageLoaded_ = true;
          return response;
        });
      });
      this.hostCallbackRouter_.exitFlow.addListener(this.exitFlow_.bind(this));

      this.controller_.bindWebviewHost(this.hostCallbackRouter_.createProxy());
    },

    /** @override */
    ready: function() {
      this.initializeLoginScreen('SupervisionOnboardingScreen', {
        commonScreenSize: true,
        resetAllowed: true,
      });
    },

    /** @private */
    onBack_: function() {
      this.controller_.handleAction(
          chromeos.supervision.mojom.OnboardingAction.kShowPreviousPage);
    },

    /** @private */
    onSkip_: function() {
      this.controller_.handleAction(
          chromeos.supervision.mojom.OnboardingAction.kSkipFlow);
    },

    /** @private */
    onNext_: function() {
      this.controller_.handleAction(
          chromeos.supervision.mojom.OnboardingAction.kShowNextPage);
    },

    /** @private */
    exitFlow_: function() {
      chrome.send(
          'login.SupervisionOnboardingScreen.userActed', ['setup-finished']);
    },
  });
}
