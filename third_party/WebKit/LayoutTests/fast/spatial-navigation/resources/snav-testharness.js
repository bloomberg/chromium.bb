(function() {
  "use strict";

  function triggerMove(direction) {
    switch (direction) {
      case 'Up':
        eventSender.keyDown('ArrowUp');
        break;
      case 'Right':
        eventSender.keyDown('ArrowRight');
        break;
      case 'Down':
        eventSender.keyDown('ArrowDown');
        break;
      case 'Left':
        eventSender.keyDown('ArrowLeft');
        break;
    }
  }

  function stepAndAssertMoves(expectedMoves, focusMoves) {
    if (expectedMoves.length == 0)
      return;

    let move = expectedMoves.shift();
    let direction = move[0];
    let expectedId = move[1];
    // TODO: Use WebDriver's API instead of eventSender.
    //       Hopefully something like:
    //         test_driver.move_focus(direction).then(...)
    async_test(t => {
      let checkFocus = t.step_func_done(function() {
        let expectedElement = document.getElementById(expectedId);
        assert_equals(document.activeElement, expectedElement);

        // Kick off another async test before closing this one.
        stepAndAssertMoves(expectedMoves, focusMoves + 1);
      });

      // "... the default action MUST be to shift the document focus".
      //   https://www.w3.org/TR/uievents/#event-type-keydown
      // Any focus movement must have happened during keydown, so once we got
      // the succeeding keyup-event, it's safe to assert activeElement.
      document.addEventListener('keyup', checkFocus, {once: true});
      triggerMove(direction);
    }, focusMoves + "th move: " + direction + '-key moves focus to ' +
       'the element with id: ' + expectedId + '.');
  }

  function assertSnavEnabledAndTestable() {
    test(() => {
      assert_true(!!window.testRunner);
      testRunner.overridePreference("WebKitTabToLinksPreferenceKey", 1);
      testRunner.overridePreference('WebKitSpatialNavigationEnabled', 1);
    }, 'window.testRunner is present.');
  }

  // TODO: Port all old spatial navigation layout tests to this method.
  window.snav = {
    assertFocusMoves: function(expectedMoves) {
      assertSnavEnabledAndTestable();
      stepAndAssertMoves(expectedMoves, 0);
    }
  }
})();
