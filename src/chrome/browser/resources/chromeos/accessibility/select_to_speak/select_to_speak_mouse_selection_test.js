// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['select_to_speak_e2e_test_base.js']);
GEN_INCLUDE(['mock_tts.js']);

/**
 * Browser tests for select-to-speak's feature to speak text
 * by holding down a key and clicking or dragging with the mouse.
 */
SelectToSpeakMouseSelectionTest = class extends SelectToSpeakE2ETest {
  constructor() {
    super();
    this.mockTts = new MockTts();
    chrome.tts = this.mockTts;
  }

  /**
   * Triggers speech using the search key and clicking with the mouse.
   * @param {Object} downEvent The mouse-down event.
   * @param {Object} upEvent The mouse-up event.
   */
  selectRangeForSpeech(downEvent, upEvent) {
    selectToSpeak.fireMockKeyDownEvent(
        {keyCode: SelectToSpeak.SEARCH_KEY_CODE});
    selectToSpeak.fireMockMouseDownEvent(downEvent);
    selectToSpeak.fireMockMouseUpEvent(upEvent);
    selectToSpeak.fireMockKeyUpEvent({keyCode: SelectToSpeak.SEARCH_KEY_CODE});
  }

  tapTrayButton(desktop, callback) {
    const button = desktop.find({
      roleType: 'button',
      attributes: {className: SELECT_TO_SPEAK_TRAY_CLASS_NAME}
    });
    callback = this.newCallback(callback);
    selectToSpeak.onStateChangeRequestedCallbackForTest_ =
        this.newCallback(() => {
          selectToSpeak.onStateChangeRequestedCallbackForTest_ = null;
          callback();
        });
    button.doDefault();
  }
};

TEST_F('SelectToSpeakMouseSelectionTest', 'SpeaksNodeWhenClicked', function() {
  this.runWithLoadedTree(
      'data:text/html;charset=utf-8,' +
          '<p>This is some text</p>',
      function(desktop) {
        assertFalse(this.mockTts.currentlySpeaking());
        assertEquals(this.mockTts.pendingUtterances().length, 0);
        this.mockTts.setOnSpeechCallbacks(
            [this.newCallback(function(utterance) {
              // Speech starts asynchronously.
              assertTrue(this.mockTts.currentlySpeaking());
              assertEquals(this.mockTts.pendingUtterances().length, 1);
              this.assertEqualsCollapseWhitespace(
                  this.mockTts.pendingUtterances()[0], 'This is some text');
            })]);
        const textNode = this.findTextNode(desktop, 'This is some text');
        const event = {
          screenX: textNode.location.left + 1,
          screenY: textNode.location.top + 1
        };
        this.selectRangeForSpeech(event, event);
      });
});

TEST_F(
    'SelectToSpeakMouseSelectionTest', 'SpeaksMultipleNodesWhenDragged',
    function() {
      this.runWithLoadedTree(
          'data:text/html;charset=utf-8,' +
              '<p>This is some text</p><p>This is some more text</p>',
          function(desktop) {
            assertFalse(this.mockTts.currentlySpeaking());
            assertEquals(this.mockTts.pendingUtterances().length, 0);
            this.mockTts.setOnSpeechCallbacks([
              this.newCallback(function(utterance) {
                assertTrue(this.mockTts.currentlySpeaking());
                assertEquals(this.mockTts.pendingUtterances().length, 1);
                this.assertEqualsCollapseWhitespace(
                    utterance, 'This is some text');
              }),
              this.newCallback(function(utterance) {
                this.assertEqualsCollapseWhitespace(
                    utterance, 'This is some more text');
              })
            ]);
            const firstNode = this.findTextNode(desktop, 'This is some text');
            const downEvent = {
              screenX: firstNode.location.left + 1,
              screenY: firstNode.location.top + 1
            };
            const lastNode =
                this.findTextNode(desktop, 'This is some more text');
            const upEvent = {
              screenX: lastNode.location.left + lastNode.location.width,
              screenY: lastNode.location.top + lastNode.location.height
            };
            this.selectRangeForSpeech(downEvent, upEvent);
          });
    });

TEST_F(
    'SelectToSpeakMouseSelectionTest', 'SpeaksAcrossNodesInAParagraph',
    function() {
      this.runWithLoadedTree(
          'data:text/html;charset=utf-8,' +
              '<p style="width:200px">This is some text in a paragraph that wraps. ' +
              '<i>Italic text</i></p>',
          function(desktop) {
            assertFalse(this.mockTts.currentlySpeaking());
            assertEquals(this.mockTts.pendingUtterances().length, 0);
            this.mockTts.setOnSpeechCallbacks(
                [this.newCallback(function(utterance) {
                  assertTrue(this.mockTts.currentlySpeaking());
                  assertEquals(this.mockTts.pendingUtterances().length, 1);
                  this.assertEqualsCollapseWhitespace(
                      utterance,
                      'This is some text in a paragraph that wraps. ' +
                          'Italic text');
                })]);
            const firstNode = this.findTextNode(
                desktop, 'This is some text in a paragraph that wraps. ');
            const downEvent = {
              screenX: firstNode.location.left + 1,
              screenY: firstNode.location.top + 1
            };
            const lastNode = this.findTextNode(desktop, 'Italic text');
            const upEvent = {
              screenX: lastNode.location.left + lastNode.location.width,
              screenY: lastNode.location.top + lastNode.location.height
            };
            this.selectRangeForSpeech(downEvent, upEvent);
          });
    });

TEST_F(
    'SelectToSpeakMouseSelectionTest', 'SpeaksNodeAfterTrayTapAndMouseClick',
    function() {
      this.runWithLoadedTree(
          'data:text/html;charset=utf-8,' +
              '<p>This is some text</p>',
          function(desktop) {
            assertFalse(this.mockTts.currentlySpeaking());
            this.mockTts.setOnSpeechCallbacks(
                [this.newCallback(function(utterance) {
                  // Speech starts asynchronously.
                  assertTrue(this.mockTts.currentlySpeaking());
                  assertEquals(this.mockTts.pendingUtterances().length, 1);
                  this.assertEqualsCollapseWhitespace(
                      this.mockTts.pendingUtterances()[0], 'This is some text');
                })]);

            const textNode = this.findTextNode(desktop, 'This is some text');
            const event = {
              screenX: textNode.location.left + 1,
              screenY: textNode.location.top + 1
            };
            // A state change request should shift us into 'selecting' state
            // from 'inactive'.
            this.tapTrayButton(desktop, () => {
              selectToSpeak.fireMockMouseDownEvent(event);
              selectToSpeak.fireMockMouseUpEvent(event);
            });
          });
    });

TEST_F(
    'SelectToSpeakMouseSelectionTest', 'CancelsSelectionModeWithStateChange',
    function() {
      this.runWithLoadedTree(
          'data:text/html;charset=utf-8,' +
              '<p>This is some text</p>',
          function(desktop) {
            const textNode = this.findTextNode(desktop, 'This is some text');
            const event = {
              screenX: textNode.location.left + 1,
              screenY: textNode.location.top + 1
            };
            // A state change request should shift us into 'selecting' state
            // from 'inactive'.
            this.tapTrayButton(desktop, () => {
              selectToSpeak.fireMockMouseDownEvent(event);
              assertEquals(SelectToSpeakState.SELECTING, selectToSpeak.state_);

              // Another state change puts us back in 'inactive'.
              this.tapTrayButton(desktop, () => {
                assertEquals(SelectToSpeakState.INACTIVE, selectToSpeak.state_);
              });
            });
          });
    });

TEST_F(
    'SelectToSpeakMouseSelectionTest', 'CancelsSpeechWithTrayTap', function() {
      this.runWithLoadedTree(
          'data:text/html;charset=utf-8,' +
              '<p>This is some text</p>',
          function(desktop) {
            assertFalse(this.mockTts.currentlySpeaking());
            assertEquals(this.mockTts.pendingUtterances().length, 0);
            this.mockTts.setOnSpeechCallbacks(
                [this.newCallback(function(utterance) {
                  // Speech starts asynchronously.
                  assertTrue(this.mockTts.currentlySpeaking());
                  assertEquals(this.mockTts.pendingUtterances().length, 1);
                  this.assertEqualsCollapseWhitespace(
                      this.mockTts.pendingUtterances()[0], 'This is some text');

                  // Cancel speech and make sure state resets to INACTIVE.
                  this.tapTrayButton(desktop, () => {
                    assertFalse(this.mockTts.currentlySpeaking());
                    assertEquals(this.mockTts.pendingUtterances().length, 0);
                    assertEquals(
                        SelectToSpeakState.INACTIVE, selectToSpeak.state_);
                  });
                })]);
            const textNode = this.findTextNode(desktop, 'This is some text');
            const event = {
              screenX: textNode.location.left + 1,
              screenY: textNode.location.top + 1
            };
            this.selectRangeForSpeech(event, event);
          });
    });

TEST_F(
    'SelectToSpeakMouseSelectionTest', 'DoesNotSpeakOnlyTheTrayButton',
    function() {
      // The tray button itself should not be spoken when clicked in selection
      // mode per UI review (but if more elements are being verbalized than just
      // the STS tray button, it may be spoken). This is similar to how the
      // stylus may act as a laser pointer unless it taps on the stylus options
      // button, which always opens on a tap regardless of the stylus behavior
      // selected.
      chrome.automation.getDesktop(this.newCallback((desktop) => {
        this.tapTrayButton(desktop, () => {
          assertEquals(selectToSpeak.state_, SelectToSpeakState.SELECTING);
          const button = desktop.find({
            roleType: 'button',
            attributes: {className: SELECT_TO_SPEAK_TRAY_CLASS_NAME}
          });

          // Use the same automation callbacks as Select-to-Speak to make
          // sure we actually don't start speech after the hittest and focus
          // callbacks are used to check which nodes should be spoken.
          desktop.addEventListener(
              EventType.MOUSE_RELEASED, this.newCallback((evt) => {
                chrome.automation.getFocus(this.newCallback((node) => {
                  assertEquals(
                      selectToSpeak.state_, SelectToSpeakState.INACTIVE);
                  assertFalse(this.mockTts.currentlySpeaking());
                  assertEquals(this.mockTts.pendingUtterances().length, 0);
                }));
              }),
              true);

          const event = {
            screenX: button.location.left + 1,
            screenY: button.location.top + 1
          };
          selectToSpeak.fireMockMouseDownEvent(event);
          selectToSpeak.fireMockMouseUpEvent(event);
        });
      }));
    });
