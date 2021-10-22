// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles automation intents for speech feedback.
 */

goog.provide('IntentHandler');

goog.require('constants');
goog.require('editing.EditableLine');
goog.require('Output');

goog.scope(function() {
const AutomationIntent = chrome.automation.AutomationIntent;
const Cursor = cursors.Cursor;
const Dir = constants.Dir;
const IntentCommandType = chrome.automation.IntentCommandType;
const IntentTextBoundaryType = chrome.automation.IntentTextBoundaryType;
const Movement = cursors.Movement;
const Range = cursors.Range;
const RoleType = chrome.automation.RoleType;
const Unit = cursors.Unit;

/**
 * A stateless class that turns intents into speech.
 */
IntentHandler = class {
  /**
   * Called when intents are received from an AutomationEvent.
   * @param {!Array<AutomationIntent>} intents
   * @param {!editing.EditableLine} cur The current line.
   * @param {editing.EditableLine} prev The previous line.
   * @return {boolean} Whether intents are handled.
   */
  static onIntents(intents, cur, prev) {
    if (intents.length === 0) {
      return false;
    }

    // Currently, discard all other intents once one is handled.
    for (let i = 0; i < intents.length; i++) {
      if (IntentHandler.onIntent(intents[i], cur, prev)) {
        return true;
      }
    }

    return false;
  }

  /**
   * Called when an intent is received.
   * @param {!AutomationIntent} intent
   * @param {!editing.EditableLine} cur The current line.
   * @param {editing.EditableLine} prev The previous line.
   * @return {boolean} Whether the intent was handled.
   */
  static onIntent(intent, cur, prev) {
    switch (intent.command) {
      case IntentCommandType.MOVE_SELECTION:
        return IntentHandler.onMoveSelection(intent, cur, prev);

        // TODO: implement support.
      case IntentCommandType.CLEAR_SELECTION:
      case IntentCommandType.DELETE:
      case IntentCommandType.DICTATE:
      case IntentCommandType.EXTEND_SELECTION:
      case IntentCommandType.FORMAT:
      case IntentCommandType.HISTORY:
      case IntentCommandType.INSERT:
      case IntentCommandType.MARKER:
      case IntentCommandType.SET_SELECTION:
        break;
    }

    return false;
  }

  /**
   * Called when the text selection moves.
   * @param {!AutomationIntent} intent A move selection
   *     intent.
   * @param {!editing.EditableLine} cur The current line.
   * @param {editing.EditableLine} prev The previous line.
   * @return {boolean} Whether the intent was handled.
   */
  static onMoveSelection(intent, cur, prev) {
    switch (intent.textBoundary) {
      case IntentTextBoundaryType.CHARACTER: {
        const text = cur.text.substring(cur.startOffset, cur.startOffset + 1);

        // First, handle the case where there is no text to the right of the
        // cursor.
        if (!text && prev) {
          // Detect cases where |cur| is immediately before an abstractSpan.
          const enteredAncestors =
              AutomationUtil.getUniqueAncestors(prev.end.node, cur.end.node);
          const exitedAncestors =
              AutomationUtil.getUniqueAncestors(cur.end.node, prev.end.node);

          // Scan up only to a root or the editable root.
          let ancestor;
          const ancestors = enteredAncestors.concat(exitedAncestors);
          while ((ancestor = ancestors.pop()) &&
                 !AutomationPredicate.rootOrEditableRoot(ancestor)) {
            const roleInfo = Output.ROLE_INFO[ancestor.role];
            if (roleInfo && roleInfo['inherits'] === 'abstractSpan') {
              // Let the caller handle this case.
              return false;
            }
          }

          // It is assumed to be a new line otherwise.
          ChromeVox.tts.speak('\n', QueueMode.CATEGORY_FLUSH);
          return true;
        }

        // Read character to the right of the cursor by building a character
        // range.
        let prevRange = null;
        if (prev) {
          prevRange = prev.createCharRange();
        }
        const newRange = cur.createCharRange();

        // Use the Output module for feedback so that we get contextual
        // information e.g. if we've entered a suggestion, insertion, or
        // deletion.
        new Output()
            .withContextFirst()
            .withRichSpeechAndBraille(
                newRange, prevRange, OutputEventType.NAVIGATE)
            .go();

        // Handled.
        return true;
      }
      case IntentTextBoundaryType.LINE_END:
      case IntentTextBoundaryType.LINE_START:
      case IntentTextBoundaryType.LINE_START_OR_END:
        cur.speakLine(prev);
        return true;

      case IntentTextBoundaryType.PARAGRAPH_START: {
        let node = cur.startContainer;

        if (node.role === RoleType.LINE_BREAK) {
          return false;
        }

        while (node && AutomationPredicate.text(node)) {
          node = node.parent;
        }

        if (!node || node.role === RoleType.TEXT_FIELD) {
          return false;
        }

        new Output()
            .withRichSpeechAndBraille(
                cursors.Range.fromNode(node), null, OutputEventType.NAVIGATE)
            .go();
        return true;
      }

      case IntentTextBoundaryType.WORD_END:
      case IntentTextBoundaryType.WORD_START: {
        const shouldMoveToPreviousWord =
            intent.textBoundary === IntentTextBoundaryType.WORD_END;
        let prevRange = null;
        if (prev) {
          prevRange = prev.createWordRange(shouldMoveToPreviousWord);
        }
        const newRange = cur.createWordRange(shouldMoveToPreviousWord);
        new Output()
            .withContextFirst()
            .withSpeech(newRange, prevRange, OutputEventType.NAVIGATE)
            .go();
        return true;
      }
        // TODO: implement support.
      case IntentTextBoundaryType.FORMAT_END:
      case IntentTextBoundaryType.FORMAT_START:
      case IntentTextBoundaryType.FORMAT_START_OR_END:
      case IntentTextBoundaryType.OBJECT:
      case IntentTextBoundaryType.PAGE_END:
      case IntentTextBoundaryType.PAGE_START:
      case IntentTextBoundaryType.PAGE_START_OR_END:
      case IntentTextBoundaryType.PARAGRAPH_END:
      case IntentTextBoundaryType.PARAGRAPH_START_OR_END:
      case IntentTextBoundaryType.SENTENCE_END:
      case IntentTextBoundaryType.SENTENCE_START:
      case IntentTextBoundaryType.SENTENCE_START_OR_END:
      case IntentTextBoundaryType.WEB_PAGE:
      case IntentTextBoundaryType.WORD_START_OR_END:
        break;
    }

    return false;
  }
};
});  // goog.scope
