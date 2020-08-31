// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides locale output services for ChromeVox, which
 * uses language information to automatically change the TTS voice.
 * Please note: we use the term 'locale' to refer to language codes e.g.
 * 'en-US'. For more information on locales:
 * https://en.wikipedia.org/wiki/Locale_(computer_software)
 */

goog.provide('LocaleOutputHelper');

goog.require('StringUtil');

LocaleOutputHelper = class {
  /** @private */
  constructor() {
    /**
     * @const
     * @private {string}
     */
    LocaleOutputHelper.BROWSER_UI_LOCALE_ =
        chrome.i18n.getUILanguage().toLowerCase();
    /** @private {string} */
    this.currentLocale_ = LocaleOutputHelper.BROWSER_UI_LOCALE_ || '';
    /**
     * Confidence threshold to meet before assigning sub-node language.
     * @const
     * @private {number}
     */
    LocaleOutputHelper.PROBABILITY_THRESHOLD_ = 0.9;
    /** @private {!Array<!TtsVoice>} */
    this.availableVoices_ = [];
    const setAvailableVoices = () => {
      chrome.tts.getVoices((voices) => {
        this.availableVoices_ = voices || [];
      });
    };
    setAvailableVoices();
    if (window.speechSynthesis) {
      window.speechSynthesis.addEventListener(
          'voiceschanged', setAvailableVoices, /* useCapture */ false);
    }
  }

  /**
   * Computes |this.currentLocale_| and |outputString|, and returns them.
   * @param {string} text
   * @param {AutomationNode} contextNode The AutomationNode that owns |text|.
   * @return {!{text: string, locale: string}}
   */
  computeTextAndLocale(text, contextNode) {
    if (!text || !contextNode) {
      return {text, locale: LocaleOutputHelper.BROWSER_UI_LOCALE_};
    }

    // Prefer the node's detected locale and fall back on the author-assigned
    // locale.
    const nodeLocale =
        contextNode.detectedLanguage || contextNode.language || '';
    const newLocale = this.computeNewLocale_(nodeLocale);
    let outputString = text;
    const shouldAlert = newLocale !== this.currentLocale_;
    if (this.hasVoiceForLocale_(newLocale)) {
      this.setCurrentLocale_(newLocale);
      if (shouldAlert) {
        // Prepend the human-readable locale to |outputString|.
        const displayLanguage =
            chrome.accessibilityPrivate.getDisplayNameForLocale(
                newLocale /* Locale to translate */,
                newLocale /* Target locale */);
        outputString =
            Msgs.getMsg('language_switch', [displayLanguage, outputString]);
      }
    } else {
      // Alert the user that no voice is available for |newLocale|.
      this.setCurrentLocale_(LocaleOutputHelper.BROWSER_UI_LOCALE_);
      const displayLanguage =
          chrome.accessibilityPrivate.getDisplayNameForLocale(
              newLocale /* Locale to translate */,
              LocaleOutputHelper.BROWSER_UI_LOCALE_ /* Target locale */);
      outputString =
          Msgs.getMsg('voice_unavailable_for_language', [displayLanguage]);
    }

    return {text: outputString, locale: this.currentLocale_};
  }

  /**
   * @param {string} nodeLocale
   * @return {string}
   * @private
   */
  computeNewLocale_(nodeLocale) {
    nodeLocale = nodeLocale.toLowerCase();
    if (LocaleOutputHelper.isValidLocale_(nodeLocale)) {
      return nodeLocale;
    }

    return LocaleOutputHelper.BROWSER_UI_LOCALE_;
  }

  /**
   * @param {string} targetLocale
   * @return {boolean}
   * @private
   */
  hasVoiceForLocale_(targetLocale) {
    const components = targetLocale.split('-');
    if (!components || components.length === 0) {
      return false;
    }

    const targetLanguage = components[0];
    for (const voice of this.availableVoices_) {
      if (!voice.lang) {
        continue;
      }
      const candidateLanguage = voice.lang.toLowerCase().split('-')[0];
      if (candidateLanguage === targetLanguage) {
        return true;
      }
    }

    return false;
  }

  /**
   * @param {string} locale
   * @private
   */
  setCurrentLocale_(locale) {
    if (LocaleOutputHelper.isValidLocale_(locale)) {
      this.currentLocale_ = locale;
    }
  }

  // =============== Static Methods ==============

  /**
   * Creates a singleton instance of LocaleOutputHelper.
   * @private
   */
  static init() {
    if (LocaleOutputHelper.instance_ !== undefined) {
      console.error(
          'LocaleOutputHelper is a singleton, can only call |init| once');
      return;
    }

    LocaleOutputHelper.instance_ = new LocaleOutputHelper();
  }

  /**
   * @return {!LocaleOutputHelper}
   */
  static get instance() {
    if (!LocaleOutputHelper.instance_) {
      LocaleOutputHelper.init();
    }
    return LocaleOutputHelper.instance_;
  }

  /**
   * @param {string} locale
   * @return {boolean}
   * @private
   */
  static isValidLocale_(locale) {
    return chrome.accessibilityPrivate.getDisplayNameForLocale(
               locale, locale) !== '';
  }
};
