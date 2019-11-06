// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('cr-focus-row-behavior-test', function() {
  /** @type {FocusableIronListItemElement} */ let testElement;

  suiteSetup(function() {
    document.body.innerHTML = `
      <dom-module id="button-three">
        <template>
          <button>
            fake button three
          </button>
        </template>
      </dom-module>

      <dom-module id="focus-row-element">
        <template>
          <div id="container" focus-row-container>
            <span>fake text</span>
            <button id="control" focus-row-control focus-type='fake-btn'>
              fake button
            </button>
            <button id="controlTwo" focus-row-control focus-type='fake-btn-two'>
              fake button two
            </button>
            <button-three focus-row-control focus-type='fake-btn-three'>
            </button-three>
          </div>
        </template>
      </dom-module>
    `;

    Polymer({
      is: 'button-three',

      /** @return {!Element} */
      getFocusableElement: function() {
        return this.$$('button');
      },
    });

    Polymer({
      is: 'focus-row-element',
      behaviors: [cr.ui.FocusRowBehavior],
      focusCallCount: 0,

      focus: function() {
        this.focusCallCount++;
      },
    });
  });

  setup(async function() {
    PolymerTest.clearBody();

    testElement = document.createElement('focus-row-element');
    document.body.appendChild(testElement);

    // Block so that FocusRowBehavior.attached can run.
    await test_util.waitAfterNextRender(testElement);
    // Wait one more time to ensure that async setup in FocusRowBehavior has
    // executed.
    await test_util.waitAfterNextRender(testElement);
  });

  test('item passes focus to first focusable child', function() {
    let focused = false;
    testElement.$.control.addEventListener('focus', function() {
      focused = true;
    });
    testElement.fire('focus');
    assertTrue(focused);
  });

  test('will focus a similar item that was last focused', function() {
    const lastButton = document.createElement('button');
    lastButton.setAttribute('focus-type', 'fake-btn-two');
    testElement.lastFocused = lastButton;

    let focused = false;
    testElement.$.controlTwo.addEventListener('focus', function() {
      focused = true;
    });
    testElement.fire('focus');
    assertTrue(focused);
  });

  test('mouse clicks on the row does not focus the controls', function() {
    let focused = false;
    testElement.$.control.addEventListener('focus', function() {
      focused = true;
    });
    MockInteractions.down(testElement);
    MockInteractions.up(testElement);
    testElement.click();
    // iron-list is responsible for firing 'focus' after taps, but is not used
    // in the test, so its necessary to manually fire 'focus' after tap.
    testElement.fire('focus');
    assertFalse(focused);
  });

  test('when focus-override is defined, returned element gains focus', () => {
    const lastButton = document.createElement('button');
    lastButton.setAttribute('focus-type', 'fake-btn-three');
    testElement.lastFocused = lastButton;

    const wait = test_util.eventToPromise('focus', testElement);
    testElement.fire('focus');
    return wait.then(() => {
      const button = getDeepActiveElement();
      assertEquals('fake button three', button.textContent.trim());
    });
  });

  test('when shift+tab pressed on first control, focus on container', () => {
    const first = testElement.$.control;
    const second = testElement.$.controlTwo;
    MockInteractions.pressAndReleaseKeyOn(first, '', 'shift', 'Tab');
    assertEquals(1, testElement.focusCallCount);
    MockInteractions.pressAndReleaseKeyOn(second, '', 'shift', 'Tab');
    assertEquals(1, testElement.focusCallCount);

    // Simulate updating a row with same first control.
    testElement.fire('dom-change');
    MockInteractions.pressAndReleaseKeyOn(first, '', 'shift', 'Tab');
    assertEquals(2, testElement.focusCallCount);
    MockInteractions.pressAndReleaseKeyOn(second, '', 'shift', 'Tab');
    assertEquals(2, testElement.focusCallCount);

    // Simulate updating row with different first control.
    first.remove();
    testElement.fire('dom-change');
    MockInteractions.pressAndReleaseKeyOn(second, '', 'shift', 'Tab');
    assertEquals(3, testElement.focusCallCount);
  });
});
