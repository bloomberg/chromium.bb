// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A TTS engine that writes to window.console.
 */

/**
 * @implements {TtsInterface}
 */
export class ConsoleTts {
  constructor() {
    /**
     * True if the console TTS is enabled by the user.
     * @type {boolean}
     * @private
     */
    this.enabled_ = false;
  }

  /** @return {!ConsoleTts} */
  static getInstance() {
    if (!ConsoleTts.instance_) {
      ConsoleTts.instance_ = new ConsoleTts();
    }
    return ConsoleTts.instance_;
  }

  /**
   * @param {string} textString
   * @param {!QueueMode} queueMode
   * @param {Object=} properties
   * @return {!ConsoleTts}
   */
  speak(textString, queueMode, properties) {
    if (this.enabled_ && window['console']) {
      let category = TtsCategory.NAV;
      if (properties && properties.category) {
        category = properties.category;
      }

      const speechLog = new SpeechLog(textString, queueMode, category);
      LogStore.getInstance().writeLog(speechLog);
      console.log(speechLog.toString());
    }
    return this;
  }

  /** @override */
  isSpeaking() {
    return false;
  }

  /** @override */
  stop() {
    if (this.enabled_) {
      console.log('Stop');
    }
  }

  /** @override */
  addCapturingEventListener(listener) {}

  /** @override */
  removeCapturingEventListener(listener) {}

  /** @override */
  increaseOrDecreaseProperty() {}

  /**
   * @param {string} property
   * @return {number}
   * @override
   */
  propertyToPercentage(property) {}

  /**
   * Sets the enabled bit.
   * @param {boolean} enabled The new enabled bit.
   */
  setEnabled(enabled) {
    this.enabled_ = enabled;
  }

  /**
   * @param {string} property
   * @return {number}
   * @override
   */
  getDefaultProperty(property) {}

  /**
   * @return {boolean}
   * @override
   */
  toggleSpeechOnOrOff() {}

  /** @override */
  resetTextToSpeechSettings() {}
}

/** @private {!ConsoleTts} */
ConsoleTts.instance_;

chrome.runtime.onMessage.addListener((message, sender, respond) => {
  if (message.target === 'ConsoleTts' && message.action === 'getInstance') {
    respond(ConsoleTts.getInstance());
  }
});
