// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview An interface for querying and modifying the global
 *     ChromeVox state, to avoid direct dependencies on the Background
 *     object and to facilitate mocking for tests.
 */

goog.provide('ChromeVoxState');
goog.provide('ChromeVoxStateObserver');

goog.require('cursors.Cursor');
goog.require('cursors.Range');
goog.require('BrailleKeyEvent');

/**
 * An interface implemented by objects that want to observe ChromeVox state
 * changes.
 * @interface
 */
ChromeVoxStateObserver = function() {};

ChromeVoxStateObserver.prototype = {
  /**
   * @param {cursors.Range} range The new range.
   */
  onCurrentRangeChanged(range) {}
};

/**
 * ChromeVox2 state object.
 * @constructor
 */
ChromeVoxState = function() {
  if (ChromeVoxState.instance) {
    throw 'Trying to create two instances of singleton ChromeVoxState.';
  }
  const backgroundWindow = chrome.extension.getBackgroundPage();
  // Only install the singleton instance if we are within the background page
  // context. Otherwise, take the instance from the background page (e.g. for
  // the panel page).
  if (backgroundWindow == window) {
    ChromeVoxState.instance = this;
  } else {
    Object.defineProperty(ChromeVoxState, 'instance', {
      get: () => {
        return backgroundWindow.ChromeVoxState.instance;
      }
    });
    return;
  }

  /** @private {!Array<!chrome.accessibilityPrivate.ScreenRect>} */
  this.focusBounds_ = [];
};

/**
 * @type {ChromeVoxState}
 */
ChromeVoxState.instance;

/**
 * Holds the un-composite tts object.
 * @type {Object}
 */
ChromeVoxState.backgroundTts;

/**
 * @type {boolean}
 */
ChromeVoxState.isReadingContinuously;

ChromeVoxState.prototype = {
  /** @type {cursors.Range} */
  get currentRange() {
    return this.getCurrentRange();
  },

  /**
   * @return {cursors.Range} The current range.
   * @protected
   */
  getCurrentRange() {
    return null;
  },

  /**
   * Return the current range, but focus recovery is not applied to it.
   * @return {cursors.Range} The current range.
   */
  getCurrentRangeWithoutRecovery: goog.abstractMethod,

  /**
   * @param {cursors.Range} newRange The new range.
   */
  setCurrentRange: goog.abstractMethod,
  /**
   * Navigate to the given range - it both sets the range and outputs it.
   * @param {!cursors.Range} range The new range.
   * @param {boolean=} opt_focus Focus the range; defaults to true.
   * @param {Object=} opt_speechProps Speech properties.
   * @param {boolean=} opt_skipSettingSelection If true, does not set
   *     the selection, otherwise it does by default.
   */
  navigateToRange: goog.abstractMethod,

  /**
   * Save the current ChromeVox range.
   */
  markCurrentRange: goog.abstractMethod,

  /**
   * Handles a braille command.
   * @param {!BrailleKeyEvent} evt
   * @param {!NavBraille} content
   * @return {boolean} True if evt was processed.
   */
  onBrailleKeyEvent: goog.abstractMethod,

  /**
   * Gets the bounds of the focus ring.
   * @return {Array<chrome.accessibilityPrivate.ScreenRect>}
   */
  getFocusBounds() {
    return this.focusBounds_;
  },

  /**
   * Sets the bounds of the focus ring.
   * @param {!Array<!chrome.accessibilityPrivate.ScreenRect>} bounds
   */
  setFocusBounds(bounds) {
    this.focusBounds_ = bounds;
    chrome.accessibilityPrivate.setFocusRings([{
      rects: bounds,
      type: chrome.accessibilityPrivate.FocusType.GLOW,
      color: constants.FOCUS_COLOR
    }]);
  }
};

/** @type {!Array<ChromeVoxStateObserver>} */
ChromeVoxState.observers = [];

/**
 * @param {ChromeVoxStateObserver} observer
 */
ChromeVoxState.addObserver = function(observer) {
  ChromeVoxState.observers.push(observer);
};

/**
 * @param {ChromeVoxStateObserver} observer
 */
ChromeVoxState.removeObserver = function(observer) {
  const index = ChromeVoxState.observers.indexOf(observer);
  if (index > -1) {
    ChromeVoxState.observers.splice(index, 1);
  }
};
