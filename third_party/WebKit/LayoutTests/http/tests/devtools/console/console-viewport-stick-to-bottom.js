// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Verifies viewport stick-to-bottom behavior.\n`);
  await TestRunner.loadModule('console_test_runner');
  await TestRunner.showPanel('console');
  await TestRunner.evaluateInPagePromise(`
      function populateConsoleWithMessages(count)
      {
          for (var i = 0; i < count - 1; ++i)
              console.log("Multiline\\nMessage #" + i);
          console.log("hello %cworld", "color: blue");
      }

      //# sourceURL=console-viewport-selection.js
    `);

  var viewportHeight = 200;
  ConsoleTestRunner.fixConsoleViewportDimensions(600, viewportHeight);
  var consoleView = Console.ConsoleView.instance();
  var viewport = consoleView._viewport;
  const minimumViewportMessagesCount = 10;
  const messagesCount = 150;
  const middleMessage = messagesCount / 2;
  var viewportMessagesCount;

  logMessagesToConsole(messagesCount, () => TestRunner.runTestSuite(testSuite));

  var testSuite = [
    function verifyViewportIsTallEnough(next) {
      viewport.invalidate();
      viewport.forceScrollItemToBeFirst(0);
      viewportMessagesCount = viewport.lastVisibleIndex() - viewport.firstVisibleIndex() + 1;
      if (viewportMessagesCount < minimumViewportMessagesCount) {
        TestRunner.addResult(String.sprintf(
            'Test cannot be run as viewport is not tall enough. It is required to contain at least %d messages, but %d only fit',
            minimumViewportMessagesCount, viewportMessagesCount));
        TestRunner.completeTest();
        return;
      }
      TestRunner.addResult(String.sprintf('Viewport contains %d messages', viewportMessagesCount));
      next();
    },

    function testScrollViewportToBottom(next) {
      consoleView._immediatelyScrollToBottom();
      dumpAndContinue(next);
    },

    function testConsoleSticksToBottom(next) {
      logMessagesToConsole(messagesCount, onMessagesDumped);

      function onMessagesDumped() {
        dumpAndContinue(next);
      }
    },

    function testEscShouldNotJumpToBottom(next) {
      viewport.setStickToBottom(false);
      viewport.element.scrollTop -= 10;
      var keyEvent = TestRunner.createKeyEvent('Escape');
      viewport._contentElement.dispatchEvent(keyEvent);
      dumpAndContinue(next);
    },

    function testChangingPromptTextShouldJumpToBottom(next) {
      TestRunner.addSniffer(Console.ConsoleView.prototype, '_promptTextChangedForTest', onContentChanged);
      var editorElement = consoleView._prompt.setText('a');

      function onContentChanged() {
        dumpAndContinue(next);
      }
    },

    function testViewportMutationsShouldPreserveStickToBottom(next) {
      viewport._contentElement.lastChild.innerText = 'More than 2 lines: foo\n\nbar';
      dumpAndContinue(onMessagesDumped);

      function onMessagesDumped() {
        viewport.setStickToBottom(false);
        viewport._contentElement.lastChild.innerText = 'More than 3 lines: foo\n\n\nbar';
        dumpAndContinue(next);
      }
    },

    function testMuteUpdatesWhileScrolling(next) {
      consoleView._updateStickToBottomOnMouseDown();
      viewport.element.scrollTop -= 10;

      TestRunner.addSniffer(Console.ConsoleView.prototype, '_scheduleViewportRefreshForTest', onMessageAdded);
      ConsoleTestRunner.evaluateInConsole('1 + 1');

      /**
             * @param {boolean} muted
             */
      function onMessageAdded(muted) {
        TestRunner.addResult('New messages were muted: ' + muted);
        TestRunner.addSniffer(
            Console.ConsoleView.prototype, '_scheduleViewportRefreshForTest', onMouseUpScheduledRefresh);
        TestRunner.addSniffer(Console.ConsoleView.prototype, '_updateViewportStickinessForTest', onUpdateStickiness);
        consoleView._updateStickToBottomOnMouseUp();
      }

      /**
             * @param {boolean} muted
             */
      function onMouseUpScheduledRefresh(muted) {
        TestRunner.addResult('Refresh was scheduled after dirty state');
      }

      function onUpdateStickiness() {
        next();
      }
    },

    function testShouldNotJumpToBottomWhenPromptFillsEntireViewport(next) {
      var text = 'Foo';
      for (var i = 0; i < viewportHeight; i++)
        text += '\n';
      Console.ConsoleView.clearConsole();
      consoleView._prompt.setText(text);
      viewport.element.scrollTop -= 10;

      var keyEvent = TestRunner.createKeyEvent('a');
      viewport._contentElement.dispatchEvent(keyEvent);
      consoleView._promptElement.dispatchEvent(new Event('input'));

      dumpAndContinue(next);
    }
  ];

  function dumpAndContinue(callback) {
    viewport.refresh();
    TestRunner.addResult(
        'Is at bottom: ' + viewport.element.isScrolledToBottom() + ', should stick: ' + viewport.stickToBottom());
    callback();
  }

  function logMessagesToConsole(count, callback) {
    var awaitingMessagesCount = count;
    function messageAdded() {
      if (!--awaitingMessagesCount)
        callback();
      else
        ConsoleTestRunner.addConsoleSniffer(messageAdded, false);
    }

    ConsoleTestRunner.addConsoleSniffer(messageAdded, false);
    TestRunner.evaluateInPage(String.sprintf('populateConsoleWithMessages(%d)', count));
  }
})();
