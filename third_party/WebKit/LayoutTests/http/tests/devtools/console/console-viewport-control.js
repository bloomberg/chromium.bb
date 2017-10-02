// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Verifies viewport correctly shows and hides messages while logging and scrolling.\n`);
  await TestRunner.loadModule('console_test_runner');
  await TestRunner.showPanel('console');
  await TestRunner.evaluateInPagePromise(`
      function addMessages(count)
      {
          for (var i = 0; i < count; ++i)
              console.log("Message #" + i);
      }

      function addRepeatingMessages(count)
      {
          for (var i = 0; i < count; ++i)
              console.log("Repeating message");
      }

      //# sourceURL=console-viewport-control.js
    `);

  const viewportHeight = 200;
  ConsoleTestRunner.fixConsoleViewportDimensions(600, viewportHeight);
  var consoleView = Console.ConsoleView.instance();
  var viewport = consoleView._viewport;
  const smallCount = 3;
  const rowHeight = viewportHeight / consoleView.minimumRowHeight();
  const maxVisibleCount = Math.ceil(rowHeight);
  const maxActiveCount = Math.ceil(rowHeight * 2);
  var wasShown = [];
  var willHide = [];

  TestRunner.addResult('Max visible messages count: ' + maxVisibleCount + ', active count: ' + maxActiveCount);

  function onMessageShown() {
    wasShown.push(this);
  }

  function onMessageHidden() {
    willHide.push(this);
  }

  function printAndResetCounts(next) {
    TestRunner.addResult('Messages shown: ' + wasShown.length + ', hidden: ' + willHide.length);
    resetShowHideCounts();
    next();
  }

  function resetShowHideCounts() {
    wasShown = [];
    willHide = [];
  }

  function logMessages(count, repeating, callback) {
    var awaitingMessagesCount = count;
    function messageAdded() {
      if (!--awaitingMessagesCount) {
        viewport.invalidate();
        callback();
      } else {
        ConsoleTestRunner.addConsoleSniffer(messageAdded, false);
      }
    }
    ConsoleTestRunner.addConsoleSniffer(messageAdded, false);
    if (!repeating)
      TestRunner.evaluateInPage(String.sprintf('addMessages(%d)', count));
    else
      TestRunner.evaluateInPage(String.sprintf('addRepeatingMessages(%d)', count));
  }

  function printStuckToBottom() {
    TestRunner.addResult(
        'Is at bottom: ' + viewport.element.isScrolledToBottom() + ', should stick: ' + viewport.stickToBottom());
  }

  function clearConsoleAndReset() {
    Console.ConsoleView.clearConsole();
    resetShowHideCounts();
  }

  TestRunner.addSniffer(Console.ConsoleViewMessage.prototype, 'wasShown', onMessageShown, true);
  TestRunner.addSniffer(Console.ConsoleViewMessage.prototype, 'willHide', onMessageHidden, true);

  TestRunner.runTestSuite([
    function addSmallCount(next) {
      clearConsoleAndReset();
      logMessages(smallCount, false, () => printAndResetCounts(next));
    },

    function addMaxVisibleCount(next) {
      clearConsoleAndReset();
      logMessages(maxVisibleCount, false, () => printAndResetCounts(next));
    },

    function addMaxActiveCount(next) {
      clearConsoleAndReset();
      logMessages(maxActiveCount, false, () => printAndResetCounts(next));
      printStuckToBottom();
    },

    function addMoreThanMaxActiveCount(next) {
      clearConsoleAndReset();
      logMessages(maxActiveCount, false, step2);
      function step2() {
        logMessages(smallCount, false, () => printAndResetCounts(next));
        printStuckToBottom();
      }
    },

    function scrollUpWithinActiveWindow(next) {
      clearConsoleAndReset();
      logMessages(maxActiveCount, false, step2);
      printStuckToBottom();
      function step2() {
        resetShowHideCounts();
        viewport.forceScrollItemToBeFirst(0);
        printAndResetCounts(next);
      }
    },

    function scrollUpToPositionOutsideOfActiveWindow(next) {
      clearConsoleAndReset();
      logMessages(maxActiveCount + smallCount, false, step2);
      printStuckToBottom();
      function step2() {
        resetShowHideCounts();
        viewport.forceScrollItemToBeFirst(0);
        printAndResetCounts(next);
      }
    },

    function logRepeatingMessages(next) {
      clearConsoleAndReset();
      logMessages(maxVisibleCount, true, () => printAndResetCounts(next));
    },

    function reorderingMessages(next) {
      clearConsoleAndReset();
      TestRunner.addResult('Logging ' + smallCount + ' messages');
      logMessages(smallCount, false, () => printAndResetCounts(step2));
      function step2() {
        resetShowHideCounts();
        TestRunner.addResult('Swapping messages 0 and 1');
        var temp = consoleView._visibleViewMessages[0];
        consoleView._visibleViewMessages[0] = consoleView._visibleViewMessages[1];
        consoleView._visibleViewMessages[1] = temp;
        viewport.invalidate();
        printAndResetCounts(next);
      }
    }
  ]);
})();
