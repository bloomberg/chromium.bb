// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../../common/testing/e2e_test_base.js']);
GEN_INCLUDE(['../../common/testing/mock_accessibility_private.js']);
GEN_INCLUDE(['../../common/testing/mock_input_ime.js']);
GEN_INCLUDE(['../../common/testing/mock_input_method_private.js']);
GEN_INCLUDE(['../../common/testing/mock_language_settings_private.js']);
GEN_INCLUDE(['../../common/testing/mock_speech_recognition_private.js']);

/**
 * Base class for tests for Dictation feature using accessibility common
 * extension browser tests.
 */
DictationE2ETestBase = class extends E2ETestBase {
  constructor() {
    super();
    this.mockAccessibilityPrivate = MockAccessibilityPrivate;
    chrome.accessibilityPrivate = this.mockAccessibilityPrivate;

    this.mockInputIme = MockInputIme;
    chrome.input.ime = this.mockInputIme;

    this.mockInputMethodPrivate = MockInputMethodPrivate;
    chrome.inputMethodPrivate = this.mockInputMethodPrivate;

    this.mockLanguageSettingsPrivate = MockLanguageSettingsPrivate;
    chrome.languageSettingsPrivate = this.mockLanguageSettingsPrivate;

    this.mockSpeechRecognitionPrivate = new MockSpeechRecognitionPrivate();
    chrome.speechRecognitionPrivate = this.mockSpeechRecognitionPrivate;

    this.iconType = this.mockAccessibilityPrivate.DictationBubbleIconType;
    this.hintType = this.mockAccessibilityPrivate.DictationBubbleHintType;

    this.dictationEngineId =
        '_ext_ime_egfdjlfmgnehecnclamagfafdccgfndpdictation';

    /** @private {!Array<Object{delay: number, callback: Function}} */
    this.setTimeoutData_ = [];

    /** @private {number} */
    this.imeContextId_ = 1;

    this.commandStrings = {
      DELETE_PREV_CHAR: 'delete',
      NAV_PREV_CHAR: 'move to the previous character',
      NAV_NEXT_CHAR: 'move to the next character',
      NAV_PREV_LINE: 'move to the previous line',
      NAV_NEXT_LINE: 'move to the next line',
      COPY_SELECTED_TEXT: 'copy',
      PASTE_TEXT: 'paste',
      CUT_SELECTED_TEXT: 'cut',
      UNDO_TEXT_EDIT: 'undo',
      REDO_ACTION: 'redo',
      SELECT_ALL_TEXT: 'select all',
      UNSELECT_TEXT: 'unselect',
      LIST_COMMANDS: 'help',
      NEW_LINE: 'new line',
    };

    // Re-initialize AccessibilityCommon with mock APIs.
    const reinit = module => {
      accessibilityCommon = new module.AccessibilityCommon();
    };
import('/accessibility_common/accessibility_common_loader.js').then(reinit);
  }

  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();

    // Wait for the Dictation module to load and set the Dictation locale.
    await importModule(
        'Dictation', '/accessibility_common/dictation/dictation.js');
    assertNotNullNorUndefined(Dictation);
    await new Promise(resolve => {
      chrome.accessibilityFeatures.dictation.set({value: true}, resolve);
    });
    await this.setPref(Dictation.DICTATION_LOCALE_PREF, 'en-US');
  }

  /** @override */
  testGenCppIncludes() {
    super.testGenCppIncludes();
    GEN(`
#include "ash/accessibility/accessibility_delegate.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "ui/accessibility/accessibility_features.h"
    `);
  }

  /** @override */
  testGenPreamble() {
    super.testGenPreamble();
    GEN(`
  base::OnceClosure load_cb =
    base::BindOnce(&ash::AccessibilityManager::SetDictationEnabled,
        base::Unretained(ash::AccessibilityManager::Get()),
        true);
    `);
    super.testGenPreambleCommon('kAccessibilityCommonExtensionId');
  }

  /** Turns on Dictation and checks IME and Speech Recognition state. */
  toggleDictationOn() {
    this.mockAccessibilityPrivate.callOnToggleDictation(true);
    assertTrue(this.getDictationActive());
    this.checkDictationImeActive();
    this.focusInputContext();
    assertTrue(this.getSpeechRecognitionActive());
  }

  /**
   * Turns Dictation off and checks IME and Speech Recognition state. Note that
   * Dictation can also be toggled off by blurring the current input context,
   * Speech recognition errors, or timeouts.
   */
  toggleDictationOff() {
    this.mockAccessibilityPrivate.callOnToggleDictation(false);
    assertFalse(
        this.getDictationActive(),
        'Dictation should be inactive after toggling Dictation');
    this.checkDictationImeInactive();
    assertFalse(
        this.getSpeechRecognitionActive(), 'Speech recognition should be off');
  }

  /** Checks that Dictation is the active IME. */
  checkDictationImeActive() {
    assertEquals(
        this.dictationEngineId,
        this.mockInputMethodPrivate.getCurrentInputMethodForTest());
    assertTrue(this.mockLanguageSettingsPrivate.hasInputMethod(
        this.dictationEngineId));
  }

  /*
   * Checks that Dictation is not the active IME.
   * @param {*} opt_activeImeId If we do not expect Dictation IME to be
   *     activated, an optional IME ID that we do expect to be activated.
   */
  checkDictationImeInactive(opt_activeImeId) {
    assertNotEquals(
        this.dictationEngineId,
        this.mockInputMethodPrivate.getCurrentInputMethodForTest());
    assertFalse(this.mockLanguageSettingsPrivate.hasInputMethod(
        this.dictationEngineId));
    if (opt_activeImeId) {
      assertEquals(
          opt_activeImeId,
          this.mockInputMethodPrivate.getCurrentInputMethodForTest());
    }
  }

  // Timeout methods.

  mockSetTimeoutMethod() {
    setTimeout = (callback, delay) => {
      // setTimeout can be called from several different sources, so track
      // them using an Array.
      this.setTimeoutData_.push({delay, callback});
    };
  }

  /** @return {?Function} */
  getCallbackWithDelay(delay) {
    for (const data of this.setTimeoutData_) {
      if (data.delay === delay) {
        return data.callback;
      }
    }

    return null;
  }

  clearSetTimeoutData() {
    this.setTimeoutData_ = [];
  }

  // Ime methods.

  focusInputContext() {
    this.mockInputIme.callOnFocus(this.imeContextId_);
  }

  blurInputContext() {
    this.mockInputIme.callOnBlur(this.imeContextId_);
  }

  /**
   * Checks that the latest IME commit text matches the expected value.
   * @param {string} expected
   */
  async assertCommittedText(expected) {
    if (!this.mockInputIme.getLastCommittedParameters()) {
      await this.mockInputIme.waitForCommit();
    }
    assertEquals(expected, this.mockInputIme.getLastCommittedParameters().text);
    assertEquals(
        this.imeContextId_,
        this.mockInputIme.getLastCommittedParameters().contextID);
  }

  // Getters and setters.

  /** @return {boolean} */
  getDictationActive() {
    return this.mockAccessibilityPrivate.getDictationActive();
  }

  /**
   * Async function to get a preference value from Settings.
   * @param {string} name
   */
  async getPref(name) {
    return new Promise(resolve => {
      chrome.settingsPrivate.getPref(name, (ret) => {
        resolve(ret);
      });
    });
  }

  /**
   * Async function to set a preference value in Settings.
   * @param {string} name
   */
  async setPref(name, value) {
    return new Promise(resolve => {
      chrome.settingsPrivate.setPref(name, value, undefined, () => {
        resolve();
      });
    });
  }

  /** @return {InputTextStrategy} */
  getInputTextStrategy() {
    return accessibilityCommon.dictation_.speechParser_.inputTextStrategy_;
  }

  /** @return {SimpleParseStrategy} */
  getSimpleParseStrategy() {
    return accessibilityCommon.dictation_.speechParser_.simpleParseStrategy_;
  }

  /** @return {PumpkinParseStrategy} */
  getPumpkinParseStrategy() {
    return accessibilityCommon.dictation_.speechParser_.pumpkinParseStrategy_;
  }

  // Speech recognition methods.

  /** @param {string} transcript */
  sendInterimSpeechResult(transcript) {
    this.mockSpeechRecognitionPrivate.fireMockOnResultEvent(
        transcript, /*is_final=*/ false);
  }

  /** @param {string} transcript */
  sendFinalSpeechResult(transcript) {
    this.mockSpeechRecognitionPrivate.fireMockOnResultEvent(
        transcript, /*is_final=*/ true);
  }

  sendSpeechRecognitionErrorEvent() {
    this.mockSpeechRecognitionPrivate.fireMockOnErrorEvent();
  }

  sendSpeechRecognitionStopEvent() {
    this.mockSpeechRecognitionPrivate.fireMockStopEvent();
  }

  /** @return {boolean} */
  getSpeechRecognitionActive() {
    return this.mockSpeechRecognitionPrivate.isStarted();
  }

  /** @return {string|undefined} */
  getSpeechRecognitionLocale() {
    return this.mockSpeechRecognitionPrivate.locale();
  }

  /** @return {boolean|undefined} */
  getSpeechRecognitionInterimResults() {
    return this.mockSpeechRecognitionPrivate.interimResults();
  }

  /**
   * @param {{
   *   clientId: (number|undefined),
   *   locale: (string|undefined),
   *   interimResults: (boolean|undefined)
   * }} properties
   */
  updateSpeechRecognitionProperties(properties) {
    this.mockSpeechRecognitionPrivate.updateProperties(properties);
  }

  // UI-related methods.

  /**
   * Waits for the updateDictationBubble() API to be called with the given
   * properties.
   * @param {DictationBubbleProperties} targetProps
   */
  async waitForUIProperties(targetProps) {
    // Poll until the updateDictationBubble() API gets called with
    // `targetProps`.
    return new Promise(resolve => {
      const printErrorMessageTimeoutId = setTimeout(() => {
        this.printErrorMessage_(targetProps);
      }, 3.5 * 1000);
      const intervalId = setInterval(() => {
        if (this.uiPropertiesMatch_(targetProps)) {
          clearTimeout(printErrorMessageTimeoutId);
          clearInterval(intervalId);
          resolve();
        }
      });
    });
  }

  /**
   * Returns true if `targetProps` matches the most recent UI properties. Must
   * match exactly.
   * @param {DictationBubbleProperties} targetProps
   * @return {boolean}
   * @private
   */
  uiPropertiesMatch_(targetProps) {
    /** @type {function(!Array<string>,!Array<string>) : boolean} */
    const areEqual = (arr1, arr2) => {
      return arr1.every((val, index) => val === arr2[index]);
    };

    const actualProps = this.mockAccessibilityPrivate.getDictationBubbleProps();
    if (!actualProps) {
      return false;
    }

    if (Object.keys(actualProps).length !== Object.keys(targetProps).length) {
      return false;
    }

    for (const key of Object.keys(targetProps)) {
      if (Array.isArray(targetProps[key]) && Array.isArray(actualProps[key])) {
        // For arrays, ensure that we compare the contents of the arrays.
        if (!areEqual(targetProps[key], actualProps[key])) {
          return false;
        }
      } else if (targetProps[key] !== actualProps[key]) {
        return false;
      }
    }

    return true;
  }

  /**
   * @param {DictationBubbleProperties} props
   * @private
   */
  printErrorMessage_(props) {
    console.error(`Still waiting for UI properties
      visible: ${props.visible}
      icon: ${props.icon}
      text: ${props.text}
      hints: ${props.hints}`);
  }
};
