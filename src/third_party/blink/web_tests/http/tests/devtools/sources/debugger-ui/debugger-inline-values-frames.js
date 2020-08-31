// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests inline values rendering while stepping between call frames.\n`);
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function testFunction()
      {
          debugger;
          var sameName = 'foo';
          innerFunction('not-foo');
      }

      function innerFunction(sameName) {
        return;
      }
  `);

  SourcesTestRunner.startDebuggerTest(runTestFunction);
  SourcesTestRunner.setQuiet(true);

  var stepCount = 0;

  function runTestFunction() {
    TestRunner.addSniffer(
        Sources.DebuggerPlugin.prototype, '_executionLineChanged',
        onSetExecutionLocation);
    TestRunner.evaluateInPage('setTimeout(testFunction, 0)');
  }

  async function onSetExecutionLocation(liveLocation) {
    TestRunner.deprecatedRunAfterPendingDispatches(dumpAndContinue.bind(
        null, this._textEditor, (await liveLocation.uiLocation()).lineNumber));
  }

  function dumpAndContinue(textEditor, lineNumber) {
    var startLine = 11;
    var endLine = 19;
    TestRunner.addResult(`=========== ${startLine}< ==========`);
    var lineCount = endLine - startLine;
    for (var i = startLine; i < endLine; ++i) {
      var output = ['[' + (i < lineCount ? ' ' : '') + i + ']'];
      output.push(i == lineNumber ? '>' : ' ');
      output.push(textEditor.line(i));
      output.push('\t');
      textEditor._decorations.get(i).forEach(decoration => output.push(decoration.element.deepTextContent()));
      TestRunner.addResult(output.join(' '));
    }

    TestRunner.addSniffer(
        Sources.DebuggerPlugin.prototype, '_executionLineChanged',
        onSetExecutionLocation);
    if (++stepCount < 6)
      SourcesTestRunner.stepInto();
    else
      SourcesTestRunner.completeDebuggerTest();
  }
})();
