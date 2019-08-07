// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests TestRunner.RuntimeAgent.evaluate can handle invalid Unicode characters.\n`);

  async function test(expression) {
    const executionContext = UI.context.flavor(SDK.ExecutionContext);
    const compileResult = await executionContext.runtimeModel.compileScript(expression, '', true, executionContext.id);
    const runResult = await executionContext.runtimeModel.runScript(compileResult.scriptId, executionContext.id);
    TestRunner.addResult(`"${expression}" -> ${runResult.object.value}`);
  }

  await test("'􏿿'==='\\u{10FFFF}'");
  await test("'\uFFFE' === '\\uFFFE'");
  await test("'\uD800' === '\\uD800'");
  await test("'􏿿'.codePointAt(0).toString(16)");
  await test("'\uD800'.codePointAt(0).toString(16)");
  await test("String.fromCodePoint(0x10ffff)");

  TestRunner.completeTest();
})();
