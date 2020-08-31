// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE([
  '../testing/chromevox_next_e2e_test_base.js', '../testing/assert_additions.js'
]);

/**
 * Test fixture for ChromeVox KeyboardHandler.
 */
ChromeVoxBackgroundKeyboardHandlerTest = class extends ChromeVoxNextE2ETest {
  /** @override */
  setUp() {
    window.keyboardHandler = new BackgroundKeyboardHandler();
  }

  createMockKeyEvent(keyCode, opt_modifiers) {
    const modifiers = opt_modifiers === undefined ? {} : opt_modifiers;
    const keyEvent = {};
    keyEvent.keyCode = keyCode;
    for (const key in modifiers) {
      keyEvent[key] = modifiers[key];
    }
    keyEvent.preventDefault = _ => {};
    keyEvent.stopPropagation = _ => {};
    return keyEvent;
  }
};


TEST_F(
    'ChromeVoxBackgroundKeyboardHandlerTest', 'SearchGetsPassedThrough',
    function() {
      this.runWithLoadedTree('<p>test</p>', function() {
        // A Search keydown gets eaten.
        const searchDown = {};
        searchDown.preventDefault = this.newCallback();
        searchDown.stopPropagation = this.newCallback();
        searchDown.metaKey = true;
        keyboardHandler.onKeyDown(searchDown);
        assertEquals(1, keyboardHandler.eatenKeyDowns_.size);

        // A Search keydown does not get eaten when there's no range.
        ChromeVoxState.instance.setCurrentRange(null);
        const searchDown2 = {};
        searchDown2.metaKey = true;
        keyboardHandler.onKeyDown(searchDown2);
        assertEquals(1, keyboardHandler.eatenKeyDowns_.size);
      });
    });

TEST_F('ChromeVoxBackgroundKeyboardHandlerTest', 'PassThroughMode', function() {
  this.runWithLoadedTree('<p>test</p>', function() {
    assertUndefined(ChromeVox.passThroughMode);
    assertEquals('no_pass_through', keyboardHandler.passThroughState_);
    assertEquals(0, keyboardHandler.eatenKeyDowns_.size);
    assertEquals(0, keyboardHandler.passedThroughKeyDowns_.size);

    // Send the pass through command: Search+Shift+Escape.
    const search = this.createMockKeyEvent(91, {metaKey: true});
    keyboardHandler.onKeyDown(search);
    assertEquals(1, keyboardHandler.eatenKeyDowns_.size);
    assertEquals(0, keyboardHandler.passedThroughKeyDowns_.size);
    assertEquals('no_pass_through', keyboardHandler.passThroughState_);
    assertUndefined(ChromeVox.passThroughMode);

    const searchShift =
        this.createMockKeyEvent(16, {metaKey: true, shiftKey: true});
    keyboardHandler.onKeyDown(searchShift);
    assertEquals(2, keyboardHandler.eatenKeyDowns_.size);
    assertEquals(0, keyboardHandler.passedThroughKeyDowns_.size);
    assertEquals('no_pass_through', keyboardHandler.passThroughState_);
    assertUndefined(ChromeVox.passThroughMode);

    const searchShiftEsc =
        this.createMockKeyEvent(27, {metaKey: true, shiftKey: true});
    keyboardHandler.onKeyDown(searchShiftEsc);
    assertEquals(3, keyboardHandler.eatenKeyDowns_.size);
    assertEquals(0, keyboardHandler.passedThroughKeyDowns_.size);
    assertEquals(
        'pending_pass_through_keyups', keyboardHandler.passThroughState_);
    assertTrue(ChromeVox.passThroughMode);

    keyboardHandler.onKeyUp(searchShiftEsc);
    assertEquals(2, keyboardHandler.eatenKeyDowns_.size);
    assertEquals(0, keyboardHandler.passedThroughKeyDowns_.size);
    assertEquals(
        'pending_pass_through_keyups', keyboardHandler.passThroughState_);
    assertTrue(ChromeVox.passThroughMode);

    keyboardHandler.onKeyUp(searchShift);
    assertEquals(1, keyboardHandler.eatenKeyDowns_.size);
    assertEquals(0, keyboardHandler.passedThroughKeyDowns_.size);
    assertEquals(
        'pending_pass_through_keyups', keyboardHandler.passThroughState_);
    assertTrue(ChromeVox.passThroughMode);

    keyboardHandler.onKeyUp(search);
    assertEquals(0, keyboardHandler.eatenKeyDowns_.size);
    assertEquals(0, keyboardHandler.passedThroughKeyDowns_.size);
    assertEquals('pending_shortcut_keyups', keyboardHandler.passThroughState_);
    assertTrue(ChromeVox.passThroughMode);

    // Now, the next series of key downs should be passed through.
    // Try Search+Ctrl+M.
    keyboardHandler.onKeyDown(search);
    assertEquals(0, keyboardHandler.eatenKeyDowns_.size);
    assertEquals(1, keyboardHandler.passedThroughKeyDowns_.size);
    assertEquals('pending_shortcut_keyups', keyboardHandler.passThroughState_);
    assertTrue(ChromeVox.passThroughMode);

    const searchCtrl =
        this.createMockKeyEvent(17, {metaKey: true, ctrlKey: true});
    keyboardHandler.onKeyDown(searchCtrl);
    assertEquals(0, keyboardHandler.eatenKeyDowns_.size);
    assertEquals(2, keyboardHandler.passedThroughKeyDowns_.size);
    assertEquals('pending_shortcut_keyups', keyboardHandler.passThroughState_);
    assertTrue(ChromeVox.passThroughMode);

    const searchCtrlM =
        this.createMockKeyEvent(77, {metaKey: true, ctrlKey: true});
    keyboardHandler.onKeyDown(searchCtrlM);
    assertEquals(0, keyboardHandler.eatenKeyDowns_.size);
    assertEquals(3, keyboardHandler.passedThroughKeyDowns_.size);
    assertEquals('pending_shortcut_keyups', keyboardHandler.passThroughState_);
    assertTrue(ChromeVox.passThroughMode);

    keyboardHandler.onKeyUp(searchCtrlM);
    assertEquals(0, keyboardHandler.eatenKeyDowns_.size);
    assertEquals(2, keyboardHandler.passedThroughKeyDowns_.size);
    assertEquals('pending_shortcut_keyups', keyboardHandler.passThroughState_);
    assertTrue(ChromeVox.passThroughMode);

    keyboardHandler.onKeyUp(searchCtrl);
    assertEquals(0, keyboardHandler.eatenKeyDowns_.size);
    assertEquals(1, keyboardHandler.passedThroughKeyDowns_.size);
    assertEquals('pending_shortcut_keyups', keyboardHandler.passThroughState_);
    assertTrue(ChromeVox.passThroughMode);

    keyboardHandler.onKeyUp(search);
    assertEquals(0, keyboardHandler.eatenKeyDowns_.size);
    assertEquals(0, keyboardHandler.passedThroughKeyDowns_.size);
    assertEquals('no_pass_through', keyboardHandler.passThroughState_);
    assertFalse(ChromeVox.passThroughMode);
  });
});

TEST_F(
    'ChromeVoxBackgroundKeyboardHandlerTest', 'PassThroughModeOff', function() {
      this.runWithLoadedTree('<p>test</p>', function() {
        function assertNoPassThrough() {
          assertUndefined(ChromeVox.passThroughMode);
          assertEquals('no_pass_through', keyboardHandler.passThroughState_);
          assertEquals(0, keyboardHandler.passedThroughKeyDowns_.size);
        }

        // Send some random keys; ensure the pass through state variables never
        // change.
        const search = this.createMockKeyEvent(91, {metaKey: true});
        keyboardHandler.onKeyDown(search);
        assertNoPassThrough();

        const searchShift =
            this.createMockKeyEvent(16, {metaKey: true, shiftKey: true});
        keyboardHandler.onKeyDown(searchShift);
        assertNoPassThrough();

        const searchShiftM =
            this.createMockKeyEvent(77, {metaKey: true, shiftKey: true});
        keyboardHandler.onKeyDown(searchShiftM);
        assertNoPassThrough();

        keyboardHandler.onKeyUp(searchShiftM);
        assertNoPassThrough();

        keyboardHandler.onKeyUp(searchShift);
        assertNoPassThrough();

        keyboardHandler.onKeyUp(search);
        assertNoPassThrough();

        keyboardHandler.onKeyDown(this.createMockKeyEvent(65));
        assertNoPassThrough();

        keyboardHandler.onKeyDown(this.createMockKeyEvent(65, {altKey: true}));
        assertNoPassThrough();

        keyboardHandler.onKeyUp(this.createMockKeyEvent(65, {altKey: true}));
        assertNoPassThrough();
      });
    });

TEST_F(
    'ChromeVoxBackgroundKeyboardHandlerTest', 'UnexpectedKeyDownUpPairs',
    function() {
      this.runWithLoadedTree('<p>test</p>', function() {
        // Send a few key downs.
        const search = this.createMockKeyEvent(91, {metaKey: true});
        keyboardHandler.onKeyDown(search);
        assertEquals(1, keyboardHandler.eatenKeyDowns_.size);

        const searchShift =
            this.createMockKeyEvent(16, {metaKey: true, shiftKey: true});
        keyboardHandler.onKeyDown(searchShift);
        assertEquals(2, keyboardHandler.eatenKeyDowns_.size);

        const searchShiftM =
            this.createMockKeyEvent(77, {metaKey: true, shiftKey: true});
        keyboardHandler.onKeyDown(searchShiftM);
        assertEquals(3, keyboardHandler.eatenKeyDowns_.size);

        // Now, send a key down, but no modifiers set, which is impossible to
        // actually press. This key is not eaten.
        const m = this.createMockKeyEvent(77, {});
        keyboardHandler.onKeyDown(m);
        assertEquals(0, keyboardHandler.eatenKeyDowns_.size);

        // To demonstrate eaten keys still work, send Search by itself, which is
        // always eaten.
        keyboardHandler.onKeyDown(search);
        assertEquals(1, keyboardHandler.eatenKeyDowns_.size);
      });
    });

TEST_F(
    'ChromeVoxBackgroundKeyboardHandlerTest',
    'UnexpectedKeyDownUpPairsPassThrough', function() {
      this.runWithLoadedTree('<p>test</p>', function() {
        // Force pass through mode.
        ChromeVox.passThroughMode = true;

        // Send a few key downs (which are passed through).
        const search = this.createMockKeyEvent(91, {metaKey: true});
        keyboardHandler.onKeyDown(search);
        assertEquals(0, keyboardHandler.eatenKeyDowns_.size);
        assertEquals(1, keyboardHandler.passedThroughKeyDowns_.size);

        const searchShift =
            this.createMockKeyEvent(16, {metaKey: true, shiftKey: true});
        keyboardHandler.onKeyDown(searchShift);
        assertEquals(0, keyboardHandler.eatenKeyDowns_.size);
        assertEquals(2, keyboardHandler.passedThroughKeyDowns_.size);

        const searchShiftM =
            this.createMockKeyEvent(77, {metaKey: true, shiftKey: true});
        keyboardHandler.onKeyDown(searchShiftM);
        assertEquals(0, keyboardHandler.eatenKeyDowns_.size);
        assertEquals(3, keyboardHandler.passedThroughKeyDowns_.size);

        // Now, send a key down, but no modifiers set, which is impossible to
        // actually press. This is passed through, so the count resets to 1.
        const m = this.createMockKeyEvent(77, {});
        keyboardHandler.onKeyDown(m);
        assertEquals(0, keyboardHandler.eatenKeyDowns_.size);
        assertEquals(1, keyboardHandler.passedThroughKeyDowns_.size);
      });
    });
