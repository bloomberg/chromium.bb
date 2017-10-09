// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Test that completions in the context of the call frame will result in names of its scope variables.\n`);
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.loadHTML(`
      <p>
      Test that completions in the context of the call frame will result in names
      of its scope variables.
      </p>
    `);
  await TestRunner.evaluateInPagePromise(`
      var a = 1;
      function testFunction()
      {
          var var1 = 2;
          var var2 = 3;
          var arr1 = [1,2,3];
          var arr2 = new Uint8Array(new ArrayBuffer(Math.pow(2, 29)));
          debugger;
      }
  `);

  SourcesTestRunner.runDebuggerTestSuite([
    function step1(next) {
      SourcesTestRunner.runTestFunctionAndWaitUntilPaused(next);
    },

    function step2(next) {
      ObjectUI.JavaScriptAutocomplete.completionsForExpression('', 'var').then(
          checkAgainstGolden.bind(this, ['var1', 'var2'], [], next));
    },

    function step3(next) {
      ObjectUI.JavaScriptAutocomplete.completionsForExpression('', 'di').then(
          checkAgainstGolden.bind(this, ['dir', 'dirxml'], [], next));
    },

    function step4(next) {
      ObjectUI.JavaScriptAutocomplete.completionsForExpression('', 'win').then(
          checkAgainstGolden.bind(this, ['window'], [], next));
    },

    function step5(next) {
      ObjectUI.JavaScriptAutocomplete.completionsForExpression('', 't').then(
          checkAgainstGolden.bind(this, ['this'], [], next));
    },

    function step6(next) {
      ObjectUI.JavaScriptAutocomplete.completionsForExpression('var1.', 'toExp')
          .then(checkAgainstGolden.bind(this, ['toExponential'], [], next));
    },

    function step7(next) {
      ObjectUI.JavaScriptAutocomplete.completionsForExpression('123.', 'toExp')
          .then(checkAgainstGolden.bind(this, [], ['toExponential'], next));
    },

    function step8(next) {
      ObjectUI.JavaScriptAutocomplete.completionsForExpression('', '').then(
          checkAgainstGolden.bind(this, [], ['$'], next));
    },

    function step9(next) {
      ObjectUI.JavaScriptAutocomplete.completionsForExpression('', '', true)
          .then(checkAgainstGolden.bind(this, ['$', 'window'], [], next));
    },

    function step10(next) {
      ObjectUI.JavaScriptAutocomplete.completionsForExpression('console.', 'log(\'bar\');')
          .then(checkAgainstGolden.bind(this, [], ['$'], next));
    },

    function step11(next) {
      ObjectUI.JavaScriptAutocomplete.completionsForExpression('arr1.', '')
          .then(checkAgainstGolden.bind(this, ['length'], ['1', '2', '3'], next));
    },

    function step12(next) {
      ObjectUI.JavaScriptAutocomplete.completionsForExpression('arr1[', '')
          .then(checkAgainstGolden.bind(this, ['"length"]'], ['3]'], next));
    },

    function step13_ShouldNotCrash(next) {
      ObjectUI.JavaScriptAutocomplete.completionsForExpression('arr2.', '')
          .then(checkAgainstGolden.bind(this, ['length'], ['1', '2', '3'], next));
    },

    function step14(next) {
      ObjectUI.JavaScriptAutocomplete.completionsForExpression('document\n', 'E')
          .then(checkAgainstGolden.bind(this, ['Element'], ['ELEMENT_NODE'], next));
    }
  ]);

  function checkAgainstGolden(golden, antiGolden, continuation, completions) {
    var suggestions = new Set(completions.map(s => s.text));
    var failed = false;
    for (var i = 0; i < golden.length; ++i) {
      if (!suggestions.has(golden[i])) {
        failed = true;
        TestRunner.addResult('FAIL: NOT FOUND: ' + golden[i]);
      }
    }

    for (var i = 0; i < antiGolden.length; ++i) {
      if (suggestions.has(antiGolden[i])) {
        failed = true;
        TestRunner.addResult('FAIL: FOUND: ' + antiGolden[i]);
      }
    }

    if (!failed)
      TestRunner.addResult('PASS');

    continuation();
  }
})();
