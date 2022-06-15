// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview An interface for querying and modifying the global
 *     ChromeVox state, to avoid direct dependencies on the Background
 *     object and to facilitate mocking for tests.
 */
import {UserActionMonitor} from '/chromevox/background/user_action_monitor.js';

/**
 * An interface implemented by objects to observe ChromeVox state changes.
 * @interface
 */
export class ChromeVoxStateObserver {
  /**
   * @param {cursors.Range} range The new range.
   * @param {boolean=} opt_fromEditing
   */
  onCurrentRangeChanged(range, opt_fromEditing) {}
}

export class ChromeVoxState {
  /** @param {ChromeVoxStateObserver} observer */
  static addObserver(observer) {
    ChromeVoxState.observers.push(observer);
  }

  /** @param {ChromeVoxStateObserver} observer */
  static removeObserver(observer) {
    const index = ChromeVoxState.observers.indexOf(observer);
    if (index > -1) {
      ChromeVoxState.observers.splice(index, 1);
    }
  }

  /** @return {cursors.Range} */
  get currentRange() {
    return this.getCurrentRange();
  }

  /**
   * @return {cursors.Range} The current range.
   * @protected
   */
  getCurrentRange() {
    return null;
  }

  /** @return {TtsBackground} */
  get backgroundTts() {
    return null;
  }

  /** @return {boolean} */
  get isReadingContinuously() {
    return false;
  }

  /** @return {cursors.Range} */
  get pageSel() {
    return null;
  }

  /** @return {boolean} */
  get talkBackEnabled() {
    return false;
  }

  /**
   * Return the current range, but focus recovery is not applied to it.
   * @return {cursors.Range} The current range.
   * @abstract
   */
  getCurrentRangeWithoutRecovery() {}

  /**
   * @param {cursors.Range} newRange The new range.
   * @param {boolean=} opt_fromEditing
   * @abstract
   */
  setCurrentRange(newRange, opt_fromEditing) {}

  /**
   * @param {TtsBackground}
   * @abstract
   */
  set backgroundTts(newBackgroundTts) {}

  /**
   * @param {boolean} newValue
   * @abstract
   */
  set isReadingContinuously(newValue) {}

  /**
   * @param {cursors.Range}
   * @abstract
   */
  set pageSel(newPageSel) {}

  /** @return {number} */
  get typingEcho() {}

  /** @param {number} newTypingEcho */
  set typingEcho(newTypingEcho) {}

  /**
   * Navigate to the given range - it both sets the range and outputs it.
   * @param {!cursors.Range} range The new range.
   * @param {boolean=} opt_focus Focus the range; defaults to true.
   * @param {Object=} opt_speechProps Speech properties.
   * @param {boolean=} opt_skipSettingSelection If true, does not set
   *     the selection, otherwise it does by default.
   * @abstract
   */
  navigateToRange(range, opt_focus, opt_speechProps, opt_skipSettingSelection) {
  }

  /**
   * Restores the last valid ChromeVox range.
   * @abstract
   */
  restoreLastValidRangeIfNeeded() {}

  /**
   * Handles a braille command.
   * @param {!BrailleKeyEvent} evt
   * @param {!NavBraille} content
   * @return {boolean} True if evt was processed.
   * @abstract
   */
  onBrailleKeyEvent(evt, content) {}

  /**
   * Forces the reading of the next change to the clipboard.
   * @abstract
   */
  readNextClipboardDataChange() {}
}

/** @type {ChromeVoxState} */
ChromeVoxState.instance;

/** @type {!Array<ChromeVoxStateObserver>} */
ChromeVoxState.observers = [];

BridgeHelper.registerHandler(
    BridgeConstants.ChromeVoxState.TARGET,
    BridgeConstants.ChromeVoxState.Action.CLEAR_CURRENT_RANGE,
    () => ChromeVoxState.instance.setCurrentRange(null));
BridgeHelper.registerHandler(
    BridgeConstants.ChromeVoxState.TARGET,
    BridgeConstants.ChromeVoxState.Action.UPDATE_PUNCTUATION_ECHO,
    echo => ChromeVoxState.instance.backgroundTts.updatePunctuationEcho(echo));
