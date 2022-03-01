// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Macro} from './macro.js';
import {MacroName} from './macro_names.js';

/**
 * Class that implements a macro to list Dictation commands (by opening a Help
 * Center article)
 */
export class ListCommandsMacro extends Macro {
  constructor() {
    super(MacroName.LIST_COMMANDS);
  }

  /** @override */
  checkContext() {
    return this.createSuccessCheckContextResult_(
        /*willImmediatelyDisambiguate=*/ false);
  }

  /** @override */
  runMacro() {
    // Note that this will open a new tab, probably ending the current
    // Dictation by changing the input focus.
    // TODO(crbug.com/1247299): This support page does not exist. Make
    // sure to get the correct URL before launch.
    window.open('https://support.google.com/chromebook?p=dictation', '_blank');
    return this.createRunMacroResult_(/*isSuccess=*/ true);
  }
}