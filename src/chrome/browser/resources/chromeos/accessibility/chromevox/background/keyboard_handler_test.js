// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['../testing/chromevox_next_e2e_test_base.js']);

/**
 * Test fixture for ChromeVox KeyboardHandler.
 * @constructor
 * @extends {ChromeVoxE2ETest}
 */
function ChromeVoxBackgroundKeyboardHandlerTest() {
  ChromeVoxNextE2ETest.call(this);
}

ChromeVoxBackgroundKeyboardHandlerTest.prototype = {
  __proto__: ChromeVoxNextE2ETest.prototype,

  /** @override */
  setUp: function() {
    window.keyboardHandler = new BackgroundKeyboardHandler();
  }
};

TEST_F(
    'ChromeVoxBackgroundKeyboardHandlerTest', 'SearchGetsPassedThrough',
    function() {
      this.runWithLoadedTree('<p>test</p>', function() {
        // A Search keydown gets eaten.
        var searchDown = {};
        searchDown.preventDefault = this.newCallback();
        searchDown.stopPropagation = this.newCallback();
        searchDown.metaKey = true;
        keyboardHandler.onKeyDown(searchDown);
        assertEquals(1, keyboardHandler.eatenKeyDowns_.size);

        // A Search keydown does not get eaten when there's no range.
        ChromeVoxState.instance.setCurrentRange(null);
        var searchDown2 = {};
        searchDown2.metaKey = true;
        keyboardHandler.onKeyDown(searchDown2);
        assertEquals(1, keyboardHandler.eatenKeyDowns_.size);
      });
    });
