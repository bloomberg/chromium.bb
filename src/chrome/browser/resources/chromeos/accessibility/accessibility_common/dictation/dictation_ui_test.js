// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['dictation_test_base.js']);

/** UI tests for Dictation. */
DictationUIE2ETest = class extends DictationE2ETestBase {};

SYNC_TEST_F(
    'DictationUIE2ETest', 'ShownWhenSpeechRecognitionStarts', async function() {
      this.toggleDictationOn();
      await this.waitForUIProperties({
        visible: true,
        icon: this.iconType.STANDBY,
      });
    });

SYNC_TEST_F(
    'DictationUIE2ETest', 'DisplaysInterimSpeechResults', async function() {
      this.toggleDictationOn();
      // Send an interim speech result.
      this.mockSpeechRecognitionPrivate.fireMockOnResultEvent(
          'Testing', /*isFinal=*/ false);
      await this.waitForUIProperties({
        visible: true,
        icon: this.iconType.HIDDEN,
        text: 'Testing',
      });
    });

SYNC_TEST_F('DictationUIE2ETest', 'DisplaysMacroSuccess', async function() {
  this.toggleDictationOn();
  // Perform a command.
  this.mockSpeechRecognitionPrivate.fireMockOnResultEvent(
      this.commandStrings.SELECT_ALL_TEXT, /*isFinal=*/ true);
  await this.waitForUIProperties({
    visible: true,
    icon: this.iconType.MACRO_SUCCESS,
    text: this.commandStrings.SELECT_ALL_TEXT,
  });
});

SYNC_TEST_F(
    'DictationUIE2ETest', 'ResetsToStandbyModeAfterFinalSpeechResult',
    async function() {
      this.toggleDictationOn();
      await this.waitForUIProperties({
        visible: true,
        icon: this.iconType.STANDBY,
      });
      // Send an interim speech result.
      this.mockSpeechRecognitionPrivate.fireMockOnResultEvent(
          'Testing', /*isFinal=*/ false);
      await this.waitForUIProperties({
        visible: true,
        icon: this.iconType.HIDDEN,
        text: 'Testing',
      });
      // Send a final speech result.
      this.mockSpeechRecognitionPrivate.fireMockOnResultEvent(
          'Testing 123', /*isFinal=*/ true);
      await this.waitForUIProperties({
        visible: true,
        icon: this.iconType.STANDBY,
      });
    });

SYNC_TEST_F(
    'DictationUIE2ETest', 'HiddenWhenDictationDeactivates', async function() {
      this.toggleDictationOn();
      await this.waitForUIProperties({
        visible: true,
        icon: this.iconType.STANDBY,
      });
      this.toggleDictationOff();
      await this.waitForUIProperties(
          {visible: false, icon: this.iconType.HIDDEN});
    });

SYNC_TEST_F('DictationUIE2ETest', 'StandbyHints', async function() {
  this.toggleDictationOn();
  await this.waitForUIProperties({
    visible: true,
    icon: this.iconType.STANDBY,
  });
  // Hints should show up after a few seconds without speech.
  await this.waitForUIProperties({
    visible: true,
    icon: this.iconType.STANDBY,
    hints: [this.hintType.TRY_SAYING, this.hintType.TYPE, this.hintType.HELP]
  });
});

SYNC_TEST_F(
    'DictationUIE2ETest', 'HintsShownWhenTextCommitted', async function() {
      this.toggleDictationOn();
      await this.waitForUIProperties({
        visible: true,
        icon: this.iconType.STANDBY,
      });

      // Send a final speech result. UI should return to standby mode.
      this.mockSpeechRecognitionPrivate.fireMockOnResultEvent(
          'Testing 123', /*isFinal=*/ true);
      await this.waitForUIProperties({
        visible: true,
        icon: this.iconType.STANDBY,
      });

      // Hints should show up after a few seconds without speech.
      await this.waitForUIProperties({
        visible: true,
        icon: this.iconType.STANDBY,
        hints: [
          this.hintType.TRY_SAYING, this.hintType.UNDO, this.hintType.DELETE,
          this.hintType.SELECT_ALL, this.hintType.HELP
        ]
      });
    });

SYNC_TEST_F(
    'DictationUIE2ETest', 'HintsShownAfterTextSelected', async function() {
      this.toggleDictationOn();
      await this.waitForUIProperties({
        visible: true,
        icon: this.iconType.STANDBY,
      });

      // Perform a select all command.
      this.mockSpeechRecognitionPrivate.fireMockOnResultEvent(
          this.commandStrings.SELECT_ALL_TEXT, /*isFinal=*/ true);
      await this.waitForUIProperties({
        visible: true,
        icon: this.iconType.MACRO_SUCCESS,
        text: this.commandStrings.SELECT_ALL_TEXT,
      });

      // Hints should show up after a few seconds without speech.
      await this.waitForUIProperties({
        visible: true,
        icon: this.iconType.STANDBY,
        hints: [
          this.hintType.TRY_SAYING, this.hintType.UNSELECT, this.hintType.COPY,
          this.hintType.DELETE, this.hintType.HELP
        ]
      });
    });

SYNC_TEST_F(
    'DictationUIE2ETest', 'HintsShownAfterCommandExecuted', async function() {
      this.toggleDictationOn();
      await this.waitForUIProperties({
        visible: true,
        icon: this.iconType.STANDBY,
      });

      // Perform a command.
      this.mockSpeechRecognitionPrivate.fireMockOnResultEvent(
          this.commandStrings.NAV_PREV_CHAR, /*isFinal=*/ true);
      await this.waitForUIProperties({
        visible: true,
        icon: this.iconType.MACRO_SUCCESS,
        text: this.commandStrings.NAV_PREV_CHAR,
      });

      // Hints should show up after a few seconds without speech.
      await this.waitForUIProperties({
        visible: true,
        icon: this.iconType.STANDBY,
        hints:
            [this.hintType.TRY_SAYING, this.hintType.UNDO, this.hintType.HELP]
      });
    });
