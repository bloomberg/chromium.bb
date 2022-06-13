// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../dictation_test_base.js']);

/** Dictation tests for Macros. */
DictationMacrosTest = class extends DictationE2ETestBase {
  constructor() {
    super();
  }
};

SYNC_TEST_F('DictationMacrosTest', 'ValidInputTextViewMacro', async function() {
  await this.waitForDictationWithCommands();
  // Toggle Dictation on so that the Macro will be runnable.
  this.toggleDictationOn(1);
  const macro = await this.getInputTextStrategy().parse('Hello world');
  assertEquals('INPUT_TEXT_VIEW', macro.getMacroNameString());
  const checkContextResult = macro.checkContext();
  assertTrue(checkContextResult.canTryAction);
  assertFalse(checkContextResult.willImmediatelyDisambiguate);
  assertEquals(undefined, checkContextResult.error);
  const runMacroResult = macro.runMacro();
  assertTrue(runMacroResult.isSuccess);
  assertEquals(undefined, runMacroResult.error);
  this.assertImeCommitParameters('Hello world', 1);
});

SYNC_TEST_F(
    'DictationMacrosTest', 'InvalidInputTextViewMacro', async function() {
      await this.waitForDictationWithCommands();
      await importModule(
          'MacroError', '/accessibility_common/dictation/macros/macro.js');
      // Do not toggle Dictation. The resulting macro will not be able to run.
      const macro = await this.getInputTextStrategy().parse('Hello world');
      assertEquals('INPUT_TEXT_VIEW', macro.getMacroNameString());
      const checkContextResult = macro.checkContext();
      assertFalse(checkContextResult.canTryAction);
      assertEquals(undefined, checkContextResult.willImmediatelyDisambiguate);
      assertEquals(MacroError.FAILED_ACTUATION, checkContextResult.error);
      const runMacroResult = macro.runMacro();
      assertFalse(runMacroResult.isSuccess);
      assertEquals(MacroError.FAILED_ACTUATION, runMacroResult.error);
    });

SYNC_TEST_F('DictationMacrosTest', 'RepeatableKeyPressMacro', async function() {
  await this.waitForDictationWithCommands();
  // DELETE_PREV_CHAR is one of many RepeatableKeyPressMacros.
  const macro = await this.getSimpleParseStrategy().parse('delete');
  assertEquals('DELETE_PREV_CHAR', macro.getMacroNameString());
  const checkContextResult = macro.checkContext();
  assertTrue(checkContextResult.canTryAction);
  assertFalse(checkContextResult.willImmediatelyDisambiguate);
  assertEquals(undefined, checkContextResult.error);
  const runMacroResult = macro.runMacro();
  assertTrue(runMacroResult.isSuccess);
  assertEquals(undefined, runMacroResult.error);
});

SYNC_TEST_F('DictationMacrosTest', 'ListCommandsMacro', async function() {
  await this.waitForDictationWithCommands();
  this.toggleDictationOn(1);
  const macro = await this.getSimpleParseStrategy().parse('help');
  assertEquals('LIST_COMMANDS', macro.getMacroNameString());
  const checkContextResult = macro.checkContext();
  assertTrue(checkContextResult.canTryAction);
  assertFalse(checkContextResult.willImmediatelyDisambiguate);
  assertEquals(undefined, checkContextResult.error);
  const runMacroResult = macro.runMacro();
  assertTrue(runMacroResult.isSuccess);
  assertEquals(undefined, runMacroResult.error);
});
