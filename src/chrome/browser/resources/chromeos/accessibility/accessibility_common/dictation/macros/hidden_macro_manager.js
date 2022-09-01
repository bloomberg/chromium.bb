// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DeletePrevSentMacro} from '/accessibility_common/dictation/macros/delete_prev_sent_macro.js';
import {MacroName} from '/accessibility_common/dictation/macros/macro_names.js';
import {DeletePrevWordMacro, NavNextWordMacro, NavPrevWordMacro} from '/accessibility_common/dictation/macros/repeatable_key_press_macro.js';
import {SmartDeletePhraseMacro} from '/accessibility_common/dictation/macros/smart_delete_phrase_macro.js';
import {SmartReplacePhraseMacro} from '/accessibility_common/dictation/macros/smart_replace_phrase_macro.js';
import {StopListeningMacro} from '/accessibility_common/dictation/macros/stop_listening_macro.js';

/**
 * Class that manages "hidden" macros e.g. macros that have been fully
 * implemented but haven't been hooked up to the speech parser.
 */
export class HiddenMacroManager {
  /** @param {!InputController} inputController */
  constructor(inputController) {
    this.inputController_ = inputController;
  }

  /** @param {!MacroName} name The macro to run. */
  runMacroForTesting(name) {
    if (!HiddenMacroManager.isHiddenMacro(name)) {
      throw new Error('HiddenMacroManager can only invoke hidden macros.');
    }

    switch (name) {
      case MacroName.STOP_LISTENING:
        new StopListeningMacro().runMacro();
        break;
      case MacroName.DELETE_PREV_WORD:
        new DeletePrevWordMacro().runMacro();
        break;
      case MacroName.DELETE_PREV_SENT:
        new DeletePrevSentMacro(this.inputController_).runMacro();
        break;
      case MacroName.NAV_NEXT_WORD:
        new NavNextWordMacro(/*isRtlLocale=*/ false).runMacro();
        break;
      case MacroName.NAV_PREV_WORD:
        new NavPrevWordMacro(/*isRtlLocale=*/ false).runMacro();
        break;
      default:
        throw new Error(`Cannot run macro: ${name} for testing`);
    }
  }

  /**
   * @param {!MacroName} name The macro to run.
   * @param {string} arg
   */
  runMacroWithStringArgForTesting(name, arg) {
    if (!HiddenMacroManager.isHiddenMacro(name)) {
      throw new Error('HiddenMacroManager can only invoke hidden macros.');
    }

    switch (name) {
      case MacroName.SMART_DELETE_PHRASE:
        new SmartDeletePhraseMacro(this.inputController_, arg).runMacro();
        break;
      default:
        throw new Error(
            `Cannot run macro: ${name} with string arg: ${arg} for testing`);
    }
  }

  /**
   * @param {!MacroName} name The macro to run.
   * @param {string} arg1
   * @param {string} arg2
   */
  runMacroWithTwoStringArgsForTesting(name, arg1, arg2) {
    if (!HiddenMacroManager.isHiddenMacro(name)) {
      throw new Error('HiddenMacroManager can only invoke hidden macros.');
    }

    switch (name) {
      case MacroName.SMART_REPLACE_PHRASE:
        new SmartReplacePhraseMacro(this.inputController_, arg1, arg2)
            .runMacro();
        break;
      default:
        throw new Error(`Cannot run macro: ${name} with string arg1: ${
            arg1} and arg2: ${arg2} for testing`);
    }
  }

  /**
   * @param {!MacroName} name
   * @return {boolean}
   */
  static isHiddenMacro(name) {
    return HiddenMacroManager.HIDDEN_MACROS_.includes(name);
  }
}

/**
 * Includes all hidden macros.
 * @const {!Array<!MacroName>}
 * @private
 */
HiddenMacroManager.HIDDEN_MACROS_ = [
  MacroName.STOP_LISTENING,
  MacroName.DELETE_PREV_WORD,
  MacroName.DELETE_PREV_SENT,
  MacroName.NAV_NEXT_WORD,
  MacroName.NAV_PREV_WORD,
  MacroName.SMART_DELETE_PHRASE,
  MacroName.SMART_REPLACE_PHRASE,
];
