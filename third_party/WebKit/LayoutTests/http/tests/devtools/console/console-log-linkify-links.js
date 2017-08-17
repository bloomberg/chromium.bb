// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Test that console.log() would linkify the links. Bug 231074.\n`);

  await TestRunner.loadModule('console_test_runner');
  await TestRunner.showPanel('console');
  await TestRunner.loadHTML(`
    <p>
    Test that console.log() would linkify the links. <a href="http://crbug.com/231074">Bug 231074.</a>
    </p>
  `);
  await TestRunner.evaluateInPagePromise(`
    console.log("http://www.chromium.org/");
    console.log("follow http://www.chromium.org/");
    console.log("string", "http://www.chromium.org/");
    console.log(123, "http://www.chromium.org/");
    console.log("http://www.chromium.org/some?v=114:56:57");
  `);

  ConsoleTestRunner.dumpConsoleMessages(false, true);

  TestRunner.addResult('Dump urls in messages');
  var consoleView = Console.ConsoleView.instance();
  var viewMessages = consoleView._visibleViewMessages;
  for (var i = 0; i < viewMessages.length; ++i) {
    var uiMessage = viewMessages[i];
    var element = uiMessage.contentElement();
    var links = element.querySelectorAll('.devtools-link');
    for (var link of links) {
      var info = Components.Linkifier._linkInfo(link);
      TestRunner.addResult('linked url:' + (info && info.url));
    }
  }

  var linkifyMe = 'at triggerError (http://localhost/show/:22:11)';
  var fragment = Console.ConsoleViewMessage._linkifyStringAsFragment(linkifyMe);
  var anchor = fragment.querySelector('.devtools-link');
  var info = Components.Linkifier._linkInfo(anchor);
  TestRunner.addResult('The string "' + linkifyMe + ' " linkifies to url: ' + (info && info.url));
  TestRunner.addResult('The lineNumber is ' + (info && info.lineNumber));
  TestRunner.addResult('The columnNumber is ' + (info && info.columnNumber));

  // Ensures urls with lots of slashes does not bog down the regex.
  Console.ConsoleViewMessage._linkifyStringAsFragment('/'.repeat(1000));
  Console.ConsoleViewMessage._linkifyStringAsFragment('/a/'.repeat(1000));

  TestRunner.completeTest();
})();
