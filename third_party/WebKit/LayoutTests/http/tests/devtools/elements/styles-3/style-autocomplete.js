// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that autocompletions are computed correctly when editing the Styles pane.\n`);
  await TestRunner.loadModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      #outer {--red-color: red}
      #middle {--blue-color: blue}
      </style>
      <div id="outer">
          <div id="middle">
              <div id="inner"></div>
          </div>
      </div>
    `);

  var node =
      ElementsTestRunner.nodeWithId('inner', node => TestRunner.cssModel.cachedMatchedCascadeForNode(node).then(step1));
  function step1(matchedStyles) {
    var namePrompt = new Elements.StylesSidebarPane.CSSPropertyPrompt(
        SDK.cssMetadata().allProperties(), matchedStyles.cssVariables(), null, true);
    var valuePrompt = valuePromptFor('color');
    function valuePromptFor(name) {
      return new Elements.StylesSidebarPane.CSSPropertyPrompt(
          SDK.cssMetadata().propertyValues(name), matchedStyles.cssVariables(), null, false);
    }
    TestRunner.runTestSuite([
      function testEmptyName(next) {
        testAgainstGolden(namePrompt, '', false, [], ['width'], next);
      },

      function testEmptyNameForce(next) {
        testAgainstGolden(namePrompt, '', true, ['width'], [], next);
      },

      function testSingleCharName(next) {
        testAgainstGolden(namePrompt, 'w', false, ['width'], [], next);
      },

      function testSubstringName(next) {
        testAgainstGolden(namePrompt, 'size', false, ['font-size', 'background-size', 'resize'], ['font-align'], next);
      },

      function testEmptyValue(next) {
        testAgainstGolden(valuePrompt, '', false, ['aliceblue', 'red', 'inherit'], [], next);
      },

      function testImportantDeclarationDoNotToggleOnExclamationMark(next) {
        testAgainstGolden(valuePrompt, 'red !', false, [], ['!important'], next);
      },

      function testImportantDeclaration(next) {
        testAgainstGolden(valuePrompt, 'red !i', false, ['!important'], [], next);
      },

      function testValueR(next) {
        testAgainstGolden(valuePrompt, 'R', false, ['RED', 'ROSYBROWN'], ['aliceblue', 'inherit'], next);
      },

      function testValueWithParenthesis(next) {
        testAgainstGolden(valuePrompt, 'saturate(0%)', false, [], ['inherit'], next);
      },

      function testValuePrefixed(next) {
        testAgainstGolden(
            valuePromptFor('-webkit-transform'), 'tr', false, ['translate', 'translateY', 'translate3d'],
            ['initial', 'inherit'], next);
      },

      function testValueUnprefixed(next) {
        testAgainstGolden(
            valuePromptFor('transform'), 'tr', false, ['translate', 'translateY', 'translate3d'],
            ['initial', 'inherit'], next);
      },

      function testValueSubstring(next) {
        testAgainstGolden(
            valuePromptFor('color'), 'blue', false, ['blue', 'darkblue', 'lightblue'],
            ['darkred', 'yellow', 'initial', 'inherit'], next);
      },

      function testNameVariables(next) {
        testAgainstGolden(namePrompt, '', true, ['--red-color', '--blue-color'], [], next);
      },

      function testValueVariables(next) {
        testAgainstGolden(valuePromptFor('color'), 'var(', true, ['--red-color)', '--blue-color)'], ['width'], next);
      }
    ]);
  }

  function testAgainstGolden(prompt, inputText, force, golden, antiGolden, callback) {
    var proxyElement = document.createElement('div');
    document.body.appendChild(proxyElement);
    proxyElement.style = 'webkit-user-select: text; -webkit-user-modify: read-write-plaintext-only';
    proxyElement.textContent = inputText;
    var selectionRange = document.createRange();
    var textNode = proxyElement.childNodes[0];
    if (textNode) {
      selectionRange.setStart(textNode, inputText.length);
      selectionRange.setEnd(textNode, inputText.length);
    } else {
      selectionRange.selectNodeContents(proxyElement);
    }
    var range = selectionRange.startContainer.rangeOfWord(
        selectionRange.startOffset, prompt._completionStopCharacters, proxyElement, 'backward');
    var prefix = range.toString();
    prompt._buildPropertyCompletions(inputText.substring(0, inputText.length - prefix.length), prefix, force)
        .then(completions);

    function completions(result) {
      var suggestions = new Set(result.map(s => s.text));
      var i;
      for (i = 0; i < golden.length; ++i) {
        if (!suggestions.has(golden[i]))
          TestRunner.addResult('NOT FOUND: ' + golden[i]);
      }
      for (i = 0; i < antiGolden.length; ++i) {
        if (suggestions.has(antiGolden[i]))
          TestRunner.addResult('FOUND: ' + antiGolden[i]);
      }
      proxyElement.remove();
      callback();
    }
  }
})();
