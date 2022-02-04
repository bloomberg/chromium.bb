// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Supported macros. Similar to UserIntent.MacroName in
 * google3/intelligence/dbw/proto/macros/user_intent.proto.
 * These should match semantic tags in Voice Access, see
 * voiceaccess_config.config and voiceaccess.patterns_template.
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 * Ensure this enum stays in sync with the CrosDictationMacroName enum in
 * tools/metrics/histograms/enums.xml.
 * @enum {number}
 */
export const MacroName = {
  UNSPECIFID: 0,

  // Simply input text into a text field.
  INPUT_TEXT_VIEW: 1,

  // Delete one character.
  DELETE_PREV_CHAR: 2,

  // Move the cursor to the previous character.
  NAV_PREV_CHAR: 3,

  // Move the cursor to the next character.
  NAV_NEXT_CHAR: 4,

  // Move up to the previous line.
  NAV_PREV_LINE: 5,

  // Move down to the next line.
  NAV_NEXT_LINE: 6,

  // Copy any selected text, using clipboard copy.
  COPY_SELECTED_TEXT: 7,

  // Paste any clipboard text.
  PASTE_TEXT: 8,

  // Cut (copy and delete) any selected text.
  CUT_SELECTED_TEXT: 9,

  // Undo previous text-editing action. Does not undo
  // previous navigation or selection action, does not
  // clear clipboard.
  UNDO_TEXT_EDIT: 10,

  // Redo previous text-editing action. Does not redo
  // previous navigation or selection action, does not
  // clear clipboard.
  REDO_ACTION: 11,

  // Select all text in the text field.
  SELECT_ALL_TEXT: 12,

  // Clears the current selection, moving the cursor to
  // the end of the selection.
  UNSELECT_TEXT: 13,

  // Lists available Dictation commands by bringing up the Help page.
  LIST_COMMANDS: 14,

  // Insert a new line character.
  // Note: This doesn't correspond to a Voice Access action.
  NEW_LINE: 15,

  // Any new actions should match with Voice Access's semantic tags.
};
