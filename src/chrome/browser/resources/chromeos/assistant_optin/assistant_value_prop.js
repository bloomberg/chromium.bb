// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design assistant
 * value prop screen.
 *
 * Event 'loading' will be fired when the page is loading/reloading.
 * Event 'error' will be fired when the webview failed to load.
 * Event 'loaded' will be fired when the page has been successfully loaded.
 */

/**
 * Name of the screen.
 * @type {string}
 */
const VALUE_PROP_SCREEN_ID = 'ValuePropScreen';

Polymer({
  is: 'assistant-value-prop',

  behaviors: [OobeI18nBehavior, OobeDialogHostBehavior],

  properties: {
    /**
     * Buttons are disabled when the webview content is loading.
     */
    buttonsDisabled: {
      type: Boolean,
      value: true,
    },

    /**
     * Default url for locale en_us.
     */
    defaultUrl: {
      type: String,
      value() {
        return this.urlTemplate_.replace('$', 'en_us');
      }
    },

    /**
     * Indicates whether user is minor mode user (e.g. under age of 18).
     */
    isMinorMode_: {
      type: Boolean,
      value: false,
    },

    /**
     * Indicates whether to use same design for accept/decline buttons.
     */
    equalWeightButtons_: {
      type: Boolean,
      value: false,
    },

    /**
     * Used to determine which activity control settings should be shown.
     */
    currentConsentStep_: {
      type: Number,
      value: 0,
    },
  },

  setUrlTemplateForTesting(url) {
    this.urlTemplate_ = url;
  },

  /**
   * The value prop URL template - loaded from loadTimeData.
   * The template is expected to have '$' instead of the locale.
   * @private {string}
   */
  urlTemplate_:
      'https://www.gstatic.com/opa-android/oobe/a02187e41eed9e42/v4_omni_$.html',

  /**
   * Whether try to reload with the default url when a 404 error occurred.
   * @type {boolean}
   * @private
   */
  reloadWithDefaultUrl_: false,

  /**
   * Whether an error occurs while the webview is loading.
   * @type {boolean}
   * @private
   */
  loadingError_: false,

  /**
   * The value prop webview object.
   * @type {Object}
   * @private
   */
  valuePropView_: null,

  /**
   * Whether the screen has been initialized.
   * @type {boolean}
   * @private
   */
  initialized_: false,

  /**
   * Whether the response header has been received for the value prop view.
   * @type {boolean}
   * @private
   */
  headerReceived_: false,

  /**
   * Whether the webview has been successfully loaded.
   * @type {boolean}
   * @private
   */
  webViewLoaded_: false,

  /**
   * Whether all the setting zippy has been successfully loaded.
   * @type {boolean}
   * @private
   */
  settingZippyLoaded_: false,

  /**
   * Whether all the consent text strings has been successfully loaded.
   * @type {boolean}
   * @private
   */
  consentStringLoaded_: false,

  /**
   * Whether the screen has been shown to the user.
   * @type {boolean}
   * @private
   */
  screenShown_: false,

  /**
   * Sanitizer used to sanitize html snippets.
   * @type {HtmlSanitizer}
   * @private
   */
  sanitizer_: new HtmlSanitizer(),

  /** @private {?assistant.BrowserProxy} */
  browserProxy_: null,

  /**
   * On-tap event handler for skip button.
   *
   * @private
   */
  onSkipTap_() {
    if (this.buttonsDisabled) {
      return;
    }
    this.buttonsDisabled = true;
    this.browserProxy_.userActed(VALUE_PROP_SCREEN_ID, ['skip-pressed']);
  },

  /**
   * On-tap event handler for next button.
   *
   * @private
   */
  onNextTap_() {
    if (this.buttonsDisabled) {
      return;
    }
    this.buttonsDisabled = true;
    this.browserProxy_.userActed(VALUE_PROP_SCREEN_ID, ['next-pressed']);
  },

  /** @override */
  created() {
    this.browserProxy_ = assistant.BrowserProxyImpl.getInstance();
  },

  /**
   * Sets learn more content text and shows it as overlay dialog.
   * @param {string} title Title of the dialog.
   * @param {string} content HTML formatted text to show.
   * @param {string} buttonText Text on the button that closes the dialog.
   */
  showLearnMoreOverlay(title, content, buttonText) {
    this.$['overlay-title-text'].innerHTML =
        this.sanitizer_.sanitizeHtml(title);
    this.$['overlay-additional-info-text'].innerHTML =
        this.sanitizer_.sanitizeHtml(content);
    this.$['overlay-close-button-text'].textContent = buttonText;
    this.$['overlay-close-button'].labelForAria = buttonText;
    this.$['learn-more-overlay'].setTitleAriaLabel(title);

    this.$['learn-more-overlay'].showModal();
    this.$['overlay-close-button'].focus();
  },

  /**
   * Hides overlay dialog.
   */
  hideOverlay() {
    this.$['learn-more-overlay'].close();
    if (this.lastFocusedElement) {
      this.lastFocusedElement.focus();
      this.lastFocusedElement = null;
    }
  },

  /**
   * Reloads value prop page by fetching setting zippy and consent string.
   */
  reloadPage() {
    this.fire('loading');

    if (this.initialized_) {
      this.browserProxy_.userActed(VALUE_PROP_SCREEN_ID, ['reload-requested']);
      this.settingZippyLoaded_ = false;
      this.consentStringLoaded_ = false;
    }

    this.reloadWebView();
    this.buttonsDisabled = true;
    this.currentConsentStep_ = 0;
  },

  /**
   * Reloads value prop animation webview.
   */
  reloadWebView() {
    this.loadingError_ = false;
    this.headerReceived_ = false;
    this.webViewLoaded_ = false;
    let locale = this.locale.replace('-', '_').toLowerCase();
    this.valuePropView_.src = this.urlTemplate_.replace('$', locale);
  },

  /**
   * Handles event when value prop webview cannot be loaded.
   */
  onWebViewErrorOccurred(details) {
    this.fire('error');
    this.loadingError_ = true;
  },

  /**
   * Handles event when value prop webview is loaded.
   */
  onWebViewContentLoad(details) {
    if (details == null) {
      return;
    }
    if (this.loadingError_ || !this.headerReceived_) {
      return;
    }
    if (this.reloadWithDefaultUrl_) {
      this.valuePropView_.src = this.defaultUrl;
      this.headerReceived_ = false;
      this.reloadWithDefaultUrl_ = false;
      return;
    }
    this.webViewLoaded_ = true;
    if (this.settingZippyLoaded_ && this.consentStringLoaded_) {
      this.onPageLoaded();
    }
  },

  /**
   * Handles event when webview request headers received.
   */
  onWebViewHeadersReceived(details) {
    if (details == null) {
      return;
    }
    this.headerReceived_ = true;
    if (details.statusCode == '404') {
      if (details.url != this.defaultUrl) {
        this.reloadWithDefaultUrl_ = true;
        return;
      } else {
        this.onWebViewErrorOccurred();
      }
    } else if (details.statusCode != '200') {
      this.onWebViewErrorOccurred();
    }
  },

  /**
   * Reload the page with the given consent string text data.
   */
  reloadContent(data) {
    this.$['value-prop-dialog'].setAttribute(
        'aria-label', data['valuePropTitle']);
    this.$['title-text'].textContent = data['valuePropTitle'];
    this.$['next-button'].labelForAria = data['valuePropNextButton'];
    this.$['next-button-text'].textContent = data['valuePropNextButton'];
    this.$['skip-button'].labelForAria = data['valuePropSkipButton'];
    this.$['skip-button-text'].textContent = data['valuePropSkipButton'];
    this.$['footer-text'].innerHTML =
        this.sanitizer_.sanitizeHtml(data['valuePropFooter']);
    this.equalWeightButtons_ = data['equalWeightButtons'];

    this.consentStringLoaded_ = true;
    if (this.settingZippyLoaded_ && this.webViewLoaded_) {
      this.onPageLoaded();
    }
  },

  /**
   * Add subtitles and setting zippys with given data.
   */
  addSettingZippy(zippy_data) {
    if (this.settingZippyLoaded_) {
      if (this.consentStringLoaded_ && this.webViewLoaded_) {
        this.onPageLoaded();
      }
      return;
    }

    // Clear containers to prevent contents being added multiple times.
    while (this.$['subtitle-container'].firstElementChild) {
      this.$['subtitle-container'].firstElementChild.remove();
    }
    while (this.$['consents-container'].firstElementChild) {
      this.$['consents-container'].firstElementChild.remove();
    }

    // `zippy_data` contains a list of lists, where each list contains the
    // setting zippys that should be shown on the same screen.
    // `isMinorMode` is the same for all data in `zippy_data`. We could use the
    // first one and set `isMinorMode_` flag.
    this.isMinorMode_ = zippy_data[0][0]['isMinorMode'];
    for (var i in zippy_data) {
      this.addSubtitle_(zippy_data[i][0], i);
      for (var j in zippy_data[i]) {
        var data = zippy_data[i][j];
        var zippy = document.createElement('setting-zippy');
        let background = this.isMinorMode_ ? '#e8f0fe' /* gblue50 */ : 'white';
        zippy.setAttribute(
            'icon-src',
            'data:text/html;charset=utf-8,' +
                encodeURIComponent(zippy.getWrappedIcon(
                    data['iconUri'], data['title'], background)));
        zippy.setAttribute('step', i);
        if (this.isMinorMode_) {
          zippy.setAttribute('hide-line', true);
          zippy.setAttribute('card-style', true);
        }

        var title = document.createElement('div');
        title.slot = 'title';
        title.innerHTML = this.sanitizer_.sanitizeHtml(data['name']);
        zippy.appendChild(title);

        var content = document.createElement('div');
        content.slot = 'content';

        var description = document.createElement('div');
        description.innerHTML =
            this.sanitizer_.sanitizeHtml(data['description']);
        description.innerHTML += '&ensp;';

        var learnMoreLink = document.createElement('a');
        learnMoreLink.textContent = data['popupLink'];
        learnMoreLink.setAttribute('href', 'javascript:void(0)');
        learnMoreLink.onclick =
            function(title, content, buttonText, focus) {
          this.lastFocusedElement = focus;
          this.showLearnMoreOverlay(title, content, buttonText);
        }.bind(this, data['learnMoreDialogTitle'],
               data['learnMoreDialogContent'], data['learnMoreDialogButton'],
               learnMoreLink);
        description.appendChild(learnMoreLink);
        content.appendChild(description);

        if (this.isMinorMode_) {
          var additionalInfo = document.createElement('div');
          additionalInfo.innerHTML =
              this.sanitizer_.sanitizeHtml(data['additionalInfo']);
          content.appendChild(document.createElement('br'));
          content.appendChild(additionalInfo);
        }

        zippy.appendChild(content);
        this.$['consents-container'].appendChild(zippy);
      }
    }
    this.showContentForStep_(this.currentConsentStep_);

    this.settingZippyLoaded_ = true;
    if (this.consentStringLoaded_ && this.webViewLoaded_) {
      this.onPageLoaded();
    }
  },

  /**
   * Add a subtitle for step with given data.
   */
  addSubtitle_(data, step) {
    var subtitle = document.createElement('div');
    subtitle.setAttribute('step', step);
    if (this.isMinorMode_) {
      var title = document.createElement('div');
      title.innerHTML = this.sanitizer_.sanitizeHtml(data['title']);
      title.classList.add('subtitle-text');
      subtitle.appendChild(title);

      var username = document.createElement('div');
      username.innerHTML = this.sanitizer_.sanitizeHtml(data['identity']);
      username.classList.add('username-text');
      subtitle.appendChild(username);
    }
    var message = document.createElement('div');
    message.innerHTML = this.sanitizer_.sanitizeHtml(data['intro']);
    message.classList.add(
        this.isMinorMode_ ? 'subtitle-message-text-minor' :
                            'subtitle-message-text');
    subtitle.appendChild(message);
    this.$['subtitle-container'].appendChild(subtitle);
  },

  /**
   * Handles event when all the page content has been loaded.
   */
  onPageLoaded() {
    this.fire('loaded');

    // The webview animation only starts playing when it is focused (in order
    // to make sure the animation and the caption are in sync).
    this.valuePropView_.focus();
    this.async(() => {
      this.buttonsDisabled = false;
      this.$['next-button'].focus();
    }, 300);

    if (!this.hidden && !this.screenShown_) {
      this.browserProxy_.screenShown(VALUE_PROP_SCREEN_ID);
      this.screenShown_ = true;
    }
  },

  /**
   * Signal from host to show the screen.
   */
  onShow() {
    this.$['overlay-close-button'].addEventListener(
        'click', () => this.hideOverlay());

    Polymer.RenderStatus.afterNextRender(
        this, () => this.$['next-button'].focus());

    if (!this.initialized_) {
      this.valuePropView_ = this.$['value-prop-view'];
      this.initializeWebview_(this.valuePropView_);
      this.reloadPage();
      this.initialized_ = true;
    }
  },

  /**
   * Update the screen to show the next settings with updated subtitle and
   * setting zippy. This is called only for minor users as settings are
   * unbundled.
   */
  showNextStep() {
    this.currentConsentStep_ += 1;
    this.showContentForStep_(this.currentConsentStep_);
    this.buttonsDisabled = false;
    this.$['next-button'].focus();
  },

  /**
   * Update visibility of subtitles and setting zippys for a given step.
   * @param {number} step
   */
  showContentForStep_(step) {
    for (let subtitle of this.$['subtitle-container'].children) {
      subtitle.hidden = subtitle.getAttribute('step') != step;
    }
    for (let zippy of this.$['consents-container'].children) {
      zippy.hidden = zippy.getAttribute('step') != step;
    }
  },

  initializeWebview_(webview) {
    const requestFilter = {urls: ['<all_urls>'], types: ['main_frame']};
    webview.request.onErrorOccurred.addListener(
        details => this.onWebViewErrorOccurred(details), requestFilter);
    webview.request.onHeadersReceived.addListener(
        details => this.onWebViewHeadersReceived(details), requestFilter);
    webview.addEventListener(
        'contentload', details => this.onWebViewContentLoad(details));
    webview.addContentScripts([webviewStripLinksContentScript]);
  },

  /**
   * Returns the webview animation container.
   */
  getAnimationContainer() {
    return this.$['animation-container'];
  },
});
