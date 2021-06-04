// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../testing/chromevox_next_e2e_test_base.js']);

GEN_INCLUDE(['../testing/fake_objects.js']);

/**
 * Test fixture for DesktopAutomationHandler.
 */
ChromeVoxDesktopAutomationHandlerTest = class extends ChromeVoxNextE2ETest {
  /** @override */
  setUp() {
    super.setUp();

    const runTest = this.deferRunTest(WhenTestDone.EXPECT);
    chrome.automation.getDesktop(desktop => {
      this.handler_ = new DesktopAutomationHandler(desktop);
      runTest();
    });
  }
};

TEST_F(
    'ChromeVoxDesktopAutomationHandlerTest', 'OnValueChangedSlider',
    function() {
      const mockFeedback = this.createMockFeedback();
      const site = `<input type="range"></input>`;
      this.runWithLoadedTree(site, function(root) {
        const slider = root.find({role: RoleType.SLIDER});
        assertTrue(!!slider);

        let sliderValue = '50%';
        Object.defineProperty(slider, 'value', {get: () => sliderValue});

        const event =
            new CustomAutomationEvent(EventType.VALUE_CHANGED, slider);
        mockFeedback.call(() => this.handler_.onValueChanged(event))
            .expectSpeech('Slider', '50%')

            // Override the min time to observe value changes so that even super
            // fast updates triggers speech.
            .call(() => DesktopAutomationHandler.MIN_VALUE_CHANGE_DELAY_MS = -1)
            .call(() => sliderValue = '60%')
            .call(() => this.handler_.onValueChanged(event))

            // The range stays on the slider, so subsequent value changes only
            // report the value.
            .expectNextSpeechUtteranceIsNot('Slider')
            .expectSpeech('60%')

            // Set the min time and send a value change which should be ignored.
            .call(
                () => DesktopAutomationHandler.MIN_VALUE_CHANGE_DELAY_MS =
                    10000)
            .call(() => sliderValue = '70%')
            .call(() => this.handler_.onValueChanged(event))

            // Send one more that is processed.
            .call(() => DesktopAutomationHandler.MIN_VALUE_CHANGE_DELAY_MS = -1)
            .call(() => sliderValue = '80%')
            .call(() => this.handler_.onValueChanged(event))

            .expectNextSpeechUtteranceIsNot('70%')
            .expectSpeech('80%')

            .replay();
      });
    });

TEST_F(
    'ChromeVoxDesktopAutomationHandlerTest', 'TaskManagerTableView',
    function() {
      const mockFeedback = this.createMockFeedback();
      this.runWithLoadedDesktop((desktop) => {
        mockFeedback
            .call(() => {
              EventGenerator.sendKeyPress(KeyCode.ESCAPE, {search: true});
            })
            .expectSpeech('Task Manager, window')
            .call(() => {
              EventGenerator.sendKeyPress(KeyCode.DOWN);
            })
            .expectSpeech('Browser', 'row 2 column 1', 'Task')
            .call(() => {
              EventGenerator.sendKeyPress(KeyCode.DOWN);
            })
            // Make sure it doesn't repeat the previous line!
            .expectNextSpeechUtteranceIsNot('Browser')
            .expectSpeech('row 3 column 1');

        mockFeedback.replay();
      });
    });

// Ensures behavior when IME candidates are selected.
TEST_F('ChromeVoxDesktopAutomationHandlerTest', 'ImeCandidate', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `<button>First</button><button>Second</button>`;
  this.runWithLoadedTree(site, function(root) {
    const candidates = root.findAll({role: RoleType.BUTTON});
    const first = candidates[0];
    const second = candidates[1];
    assertNotNullNorUndefined(first);
    assertNotNullNorUndefined(second);
    // Fake roles to imitate IME candidates.
    Object.defineProperty(first, 'role', {get: () => RoleType.IME_CANDIDATE});
    Object.defineProperty(second, 'role', {get: () => RoleType.IME_CANDIDATE});
    const selectFirst = new CustomAutomationEvent(EventType.SELECTION, first);
    const selectSecond = new CustomAutomationEvent(EventType.SELECTION, second);
    mockFeedback.call(() => this.handler_.onSelection(selectFirst))
        .expectSpeech('First')
        .expectSpeech('F: foxtrot, i: india, r: romeo, s: sierra, t: tango')
        .call(() => this.handler_.onSelection(selectSecond))
        .expectSpeech('Second')
        .expectSpeech(
            'S: sierra, e: echo, c: charlie, o: oscar, n: november, d: delta')
        .call(() => this.handler_.onSelection(selectFirst))
        .expectSpeech('First')
        .expectSpeech(/foxtrot/)
        .replay();
  });
});

TEST_F(
    'ChromeVoxDesktopAutomationHandlerTest', 'IgnoreRepeatedAlerts',
    function() {
      const mockFeedback = this.createMockFeedback();
      const site = `<button>Hello world</button>`;
      this.runWithLoadedTree(site, function(root) {
        const button = root.find({role: RoleType.BUTTON});
        assertTrue(!!button);
        const event = new CustomAutomationEvent(EventType.ALERT, button);
        mockFeedback
            .call(() => {
              DesktopAutomationHandler.MIN_ALERT_DELAY_MS = 20 * 1000;
              this.handler_.onAlert(event);
            })
            .expectSpeech('Hello world')
            .clearPendingOutput()
            .call(() => {
              // Repeated alerts should be ignored.
              this.handler_.onAlert(event);
              assertFalse(mockFeedback.utteranceInQueue('Hello world'));
              this.handler_.onAlert(event);
              assertFalse(mockFeedback.utteranceInQueue('Hello world'));
            })
            .replay();
      });
    });
