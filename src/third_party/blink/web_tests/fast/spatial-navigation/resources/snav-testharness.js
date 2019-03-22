(function() {
  "use strict";

  let gAsyncTest;

  // TODO: Use WebDriver's API instead of eventSender.
  //       Hopefully something like:
  //         test_driver.move_focus(direction).then(...)
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
      case 'Forward':
        eventSender.keyDown('Tab');
        break;
      case 'Backward':
        eventSender.keyDown('Tab', ['shiftKey']);
        break;
    }
  }

  function focusedDocument(currentDocument=document) {
    let subframes = Array.from(currentDocument.defaultView.frames);
    for (let frame of subframes) {
      let focused = focusedDocument(frame.document);
      if (focused)
        return focused;
    }
    if (currentDocument.hasFocus())
      return currentDocument;
    return null;
  }

  // Allows us to query element ids also in iframes' documents.
  function findElement(searchString) {
    let searchPath = searchString.split(",");
    let currentDocument = document;
    let elem = undefined;

    while (searchPath.length > 0) {
      let id = searchPath.shift();
      elem = currentDocument.getElementById(id);
      assert_not_equals(elem, null, 'Could not find "' + searchString + '",');

      if (elem.tagName == "IFRAME")
        currentDocument = elem.contentDocument;
    }
    return elem;
  }

  function stepAndAssertMoves(expectedMoves) {
    if (expectedMoves.length == 0) {
      gAsyncTest.done();
      return;
    }

    let move = expectedMoves.shift();
    let direction = move[0];
    let expectedId = move[1];
    let wanted = findElement(expectedId);
    let receivingDoc = wanted.ownerDocument;
    let verifyAndAdvance = gAsyncTest.step_func(function() {
      let focused = focusedDocument().activeElement;
      assert_equals(focused, wanted);
      // Kick off another async test step.
      stepAndAssertMoves(expectedMoves);
    });

    // "... the default action MUST be to shift the document focus".
    //   https://www.w3.org/TR/uievents/#event-type-keydown
    // Any focus movement must have happened during keydown, so once we got
    // the succeeding keyup-event, it's safe to assert activeElement.
    // The keyup-event targets the, perhaps newly, focused document.
    receivingDoc.addEventListener('keyup', verifyAndAdvance, {once: true});
    triggerMove(direction);
  }

  // TODO: Port all old spatial navigation layout tests to this method.
  window.snav = {
    assertSnavEnabledAndTestable: function() {
      test(() => {
        assert_true(!!window.testRunner);
        testRunner.overridePreference("WebKitTabToLinksPreferenceKey", 1);
        testRunner.overridePreference('WebKitSpatialNavigationEnabled', 1);
      }, 'window.testRunner is present.');
    },

    assertFocusMoves: function(expectedMoves, enableSpatnav=true) {
      if (enableSpatnav)
        snav.assertSnavEnabledAndTestable();
      gAsyncTest = async_test("Focus movements:\n" +
          JSON.stringify(expectedMoves).replace(/],/g, ']\n') + '\n');

      // All iframes must be loaded before trying to navigate to them.
      window.addEventListener('load', gAsyncTest.step_func(() => {
        stepAndAssertMoves(expectedMoves);
      }));
    }
  }
})();
