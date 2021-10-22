// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{
 *   contextID: number,
 * }}
 */
let InputContext;

/**
 * @typedef {{
 *   contextID: number,
 *   text: string,
 *   cursor: number,
 * }}
 */
let MockImeCompositionParameters;

/**
 * @typedef {{
 *   contextID: number,
 *   text: string,
 * }}
 */
let MockImeCommitParameters;

/*
 * A mock chrome.input.ime API for tests.
 */
var MockInputIme = {
  /** @private {function<InputContext>} */
  onFocusListener_: null,

  /** @private {function<number>} */
  onBlurListener_: null,

  /** @private {MockImeCompositionParameters} */
  lastCompositionParameters_: null,

  /** @private {MockImeCommitParameters} */
  lastCommittedParameters_: null,

  // Methods from chrome.input.ime API. //

  onFocus: {
    /**
     * Adds a listener to onFocus.
     * @param {function<InputContext>} listener
     */
    addListener: (listener) => {
      MockInputIme.onFocusListener_ = listener;
    },

    /**
     * Removes the listener.
     * @param {function<InputContext>} listener
     */
    removeListener: (listener) => {
      if (MockInputIme.onFocusListener_ === listener) {
        MockInputIme.onFocusListener_ = null;
      }
    }
  },

  onBlur: {
    /**
     * Adds a listener to onBlur.
     * @param {function<number>} listener
     */
    addListener: (listener) => {
      MockInputIme.onBlurListener_ = listener;
    },

    /**
     * Removes the listener.
     * @param {function<number>} listener
     */
    removeListener: (listener) => {
      if (MockInputIme.onBlurListener_ === listener) {
        MockInputIme.onBlurListener_ = null;
      }
    }
  },

  /** @param {!MockImeCompositionParameters} composition */
  setComposition(composition) {
    MockInputIme.lastCompositionParameters_ = composition;
  },

  /** Clears composition text. */
  clearComposition() {
    MockInputIme.lastCompositionParameters_ = null;
  },

  /** @param {!MockImeCommitParameters} commitParameters */
  commitText(commitParameters) {
    MockInputIme.lastCommittedParameters_ = commitParameters;
  },

  /** @param {Object} unused */
  setCandidateWindowProperties(unused) {},

  /** @param {Object} unused */
  setCandidates(unused) {},

  // Methods for testing. //

  /**
   * Calls listeners for chrome.input.ime.onFocus with a InputContext with the
   * given contextID.
   * @param {number} contextID
   */
  callOnFocus(contextID) {
    if (MockInputIme.onFocusListener_) {
      MockInputIme.onFocusListener_({contextID});
    }
  },

  /**
   * Calls listeners for chrome.input.ime.onBlur with the given contextID.
   * @param {number} contextID
   */
  callOnBlur(contextID) {
    if (MockInputIme.onBlurListener_) {
      MockInputIme.onBlurListener_(contextID);
    }
  },

  /**
   * Gets the most recently set composition parameters.
   * @return {MockImeCompositionParameters}
   */
  getLastCompositionParameters() {
    return MockInputIme.lastCompositionParameters_;
  },

  /**
   * Gets the most recently committed parameters.
   * @return {MockImeCommitParameters}
   */
  getLastCommittedParameters() {
    return MockInputIme.lastCommittedParameters_;
  },

  /** Resets composition and committed parameters. */
  clearLastParameters() {
    MockInputIme.lastCommittedParameters_ = null;
    MockInputIme.lastCompositionParameters_ = null;
  },
};
