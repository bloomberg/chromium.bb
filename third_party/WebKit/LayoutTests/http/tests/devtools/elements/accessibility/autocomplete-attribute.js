// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that autocompletions are computed correctly when editing the ARIA pane.\n`);
  await TestRunner.loadModule('elements_test_runner');
  await TestRunner.loadModule('accessibility_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <span id="inspected" aria-checked="true" role="checkbox"></span>
    `);

  UI.viewManager.showView('accessibility.view')
      .then(() => AccessibilityTestRunner.selectNodeAndWaitForAccessibility('inspected'))
      .then(runTests);

  function getPromptForAttribute(attribute) {
    var treeElement = AccessibilityTestRunner.findARIAAttributeTreeElement(attribute);
    treeElement._startEditing();
    return treeElement._prompt;
  }

  function runTests() {
    TestRunner.runTestSuite([
      function testCheckedEmptyValue(next) {
        var prompt = getPromptForAttribute('aria-checked');
        testAgainstGolden(prompt, '', ['true', 'false', 'mixed'], next);
      },

      function testCheckedFirstCharacter(next) {
        var prompt = getPromptForAttribute('aria-checked');
        testAgainstGolden(prompt, 't', ['true'], next);
      },

      function testRoleFirstCharacter(next) {
        var prompt = getPromptForAttribute('role');
        testAgainstGolden(prompt, 'b', ['banner', 'button'], next);
      }
    ]);
  }

  function testAgainstGolden(prompt, inputText, golden, callback) {
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
    prompt._buildPropertyCompletions(inputText.substring(0, inputText.length - prefix.length), prefix, true)
        .then(completions);

    function completions(result) {
      var suggestions = new Set(result.map(s => s.text));
      var i;
      for (i = 0; i < golden.length; ++i) {
        if (!suggestions.has(golden[i]))
          TestRunner.addResult('NOT FOUND: ' + golden[i]);
      }
      proxyElement.remove();
      callback();
    }
  }
})();
