(async function() {
  await TestRunner.loadModule('formatter');
  await TestRunner.loadModule('object_ui');
  TestRunner.addResult('Test side effect check requirement tester.');

  await check(`var x`, true);
  await check(`import x`, true);
  await check(`new x()`, true);
  await check(`for (;;) {}`, true);
  await check(`while () {}`, true);
  await check(`123`, false);
  await check(`1+2`, false);
  await check(`x++`, true);
  await check(`foo+name`, false);
  await check(`foo()+name`, true);
  await check(`foo();name`, true);
  await check(`name`, false);
  await check(`name[0]`, false);
  await check(`name[1+2]`, false);
  await check(`name.foo.bar`, false);
  await check(`name.foo[0].bar`, false);
  await check(`name.foo().bar`, true);
  await check(`syntax_error,,`, true);

  TestRunner.completeTest();

  async function check(text, expected) {
    const actual = (await Formatter.formatterWorkerPool().hasPossibleSideEffects(text));
    const differ = actual !== expected;
    TestRunner.addResult(`Expression "${text}"\n${differ ? 'FAIL: ' : 'PASS: '}needsSideEffectCheck: ${actual}, expected: ${expected}`);
  }
})();
