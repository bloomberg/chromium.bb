// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Macro} from '/accessibility_common/dictation/macros/macro.js';
import {MacroName} from '/accessibility_common/dictation/macros/macro_names.js';

/** Class that implements a macro to stop Dictation. */
export class StopListeningMacro extends Macro {
  constructor() {
    super(MacroName.STOP_LISTENING);
  }

  /** @override */
  checkContext() {
    return this.createSuccessCheckContextResult_(
        /*willImmediatelyDisambiguate=*/ false);
  }

  /** @override */
  runMacro() {
    chrome.accessibilityPrivate.toggleDictation();
    return this.createRunMacroResult_(/*isSuccess=*/ true);
  }
}
