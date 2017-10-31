// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that ObjectPropertiesSection fires callback upon DOM changes.\n`);

  await TestRunner.loadModule('object_ui');
  await TestRunner.showPanel('console');

  TestRunner.addSniffer(Console.ConsoleViewMessage.prototype, '_onObjectChange', onChange, true /* sticky */);
  await TestRunner.evaluateInPagePromise(`
    var arrayWithRanges = new Array(101).fill(1);
    var localObject = {a: arrayWithRanges, get b() { return 'myGetterValue'; } };
    console.log(localObject);
  `);

  var messageElement = Console.ConsoleView._instance._visibleViewMessages[0].element();
  var section = messageElement.querySelector('.console-view-object-properties-section')._section;
  var rootElement = section.objectTreeElement();

  var expectedChangeCount;
  var actualChangeCount = 0;
  var waitForConditionCallback;
  var continueCondition = () => false;

  TestRunner.runTestSuite([
    function expand(next) {
      waitForCondition(() => (rootElement.children().length === 4), next);
      section.expand();
    },
    function expandArrayWithRanges(next) {
      var arrayPropertyTreeElement = rootElement.childAt(0);
      waitForCondition(() => (arrayPropertyTreeElement.children().length === 4), next);
      arrayPropertyTreeElement.expand();
    },
    function expandArrayRange(next) {
      var arrayRangeTreeElement = rootElement.childAt(0).childAt(0);
      waitForCondition(() => (arrayRangeTreeElement.children().length === 100), next);
      arrayRangeTreeElement.expand();
    },
    function clickAccessorProperty(next) {
      var getterTreeElement = rootElement.childAt(1);
      waitForCondition(() => (getterTreeElement.valueElement.textContent !== '(...)'), next);
      getterTreeElement.listItemElement.querySelector('.object-value-calculate-value-button').click();
    },
    function applyExpression(next) {
      var getterTreeElement = rootElement.childAt(1);
      waitForCondition(() => (getterTreeElement.valueElement.textContent !== 'myGetterValue'), next);
      getterTreeElement._applyExpression('1 + 1');
    },
    function collapse(next) {
      waitForCondition(() => (!rootElement.treeOutline.element.classList.contains('expanded')), next);
      rootElement.collapse();
    }
  ]);

  /**
   * @param {function():boolean} condition
   * @param {function()} next
   */
  function waitForCondition(condition, next) {
    continueCondition = condition;
    waitForConditionCallback = next;
  }

  function onChange() {
    actualChangeCount++;
    if (waitForConditionCallback && continueCondition()) {
      var callback = waitForConditionCallback;
      // Flush any pending synchronous onChange calls before proceeding.
      waitForConditionCallback = null;
      setTimeout(() => {
        TestRunner.addResult(actualChangeCount + ' change calls fired');
        actualChangeCount = 0;
        callback();
      });
    }
  }
})();
