export const description = `
Tests for getStackTrace.
`;

import { TestGroup, getStackTrace } from '../../framework/index.js';

import { UnitTest } from './unit_test.js';

export const g = new TestGroup(UnitTest);

g.test('stacks', t => {
  const lines: number = t.params._expectedLines;

  const ex = new Error();
  ex.stack = t.params._stack;
  t.expect(ex.stack === t.params._stack);
  const stringified = getStackTrace(ex);
  const parts = stringified.split('\n');

  t.expect(parts.length === lines);
  t.expect(parts.every(p => p.indexOf('/suites/') !== -1));
}).params([
  {
    case: 'node_fail',
    _expectedLines: 1,
    _stack: `Error:
   at CaseRecorder.fail (/Users/kainino/src/cts-experiment/src/framework/logger.ts:99:30)
   at RunCaseSpecific.exports.g.test.t [as fn] (/Users/kainino/src/cts-experiment/src/suites/unittests/logger.spec.ts:80:7)
   at RunCaseSpecific.run (/Users/kainino/src/cts-experiment/src/framework/test_group.ts:121:18)
   at processTicksAndRejections (internal/process/task_queues.js:86:5)`,
  },
  {
    case: 'node_throw',
    _expectedLines: 1,
    _stack: `Error: hello
    at RunCaseSpecific.g.test.t [as fn] (/Users/kainino/src/cts-experiment/src/suites/unittests/test_group.spec.ts:51:11)
    at RunCaseSpecific.run (/Users/kainino/src/cts-experiment/src/framework/test_group.ts:121:18)
    at processTicksAndRejections (internal/process/task_queues.js:86:5)`,
  },
  {
    case: 'firefox_fail',
    _expectedLines: 1,
    _stack: `fail@http://localhost:8080/out/framework/logger.js:104:30
expect@http://localhost:8080/out/framework/default_fixture.js:59:16
@http://localhost:8080/out/suites/unittests/util.spec.js:35:5
run@http://localhost:8080/out/framework/test_group.js:119:18`,
  },
  {
    case: 'firefox_throw',
    _expectedLines: 1,
    _stack: `@http://localhost:8080/out/suites/unittests/test_group.spec.js:48:11
run@http://localhost:8080/out/framework/test_group.js:119:18`,
  },
  {
    case: 'safari_fail',
    _expectedLines: 1,
    _stack: `fail@http://localhost:8080/out/framework/logger.js:104:39
expect@http://localhost:8080/out/framework/default_fixture.js:59:20
http://localhost:8080/out/suites/unittests/util.spec.js:35:11
http://localhost:8080/out/framework/test_group.js:119:20
asyncFunctionResume@[native code]
[native code]
promiseReactionJob@[native code]`,
  },
  {
    case: 'safari_throw',
    _expectedLines: 1,
    _stack: `http://localhost:8080/out/suites/unittests/test_group.spec.js:48:20
http://localhost:8080/out/framework/test_group.js:119:20
asyncFunctionResume@[native code]
[native code]
promiseReactionJob@[native code]`,
  },
  {
    case: 'chrome_fail',
    _expectedLines: 1,
    _stack: `Error
    at CaseRecorder.fail (http://localhost:8080/out/framework/logger.js:104:30)
    at DefaultFixture.expect (http://localhost:8080/out/framework/default_fixture.js:59:16)
    at RunCaseSpecific.fn (http://localhost:8080/out/suites/unittests/util.spec.js:35:5)
    at RunCaseSpecific.run (http://localhost:8080/out/framework/test_group.js:119:18)
    at async runCase (http://localhost:8080/out/runtime/standalone.js:37:17)
    at async http://localhost:8080/out/runtime/standalone.js:102:7`,
  },
  {
    case: 'chrome_throw',
    _expectedLines: 1,
    _stack: `Error: hello
    at RunCaseSpecific.fn (http://localhost:8080/out/suites/unittests/test_group.spec.js:48:11)
    at RunCaseSpecific.run (http://localhost:8080/out/framework/test_group.js:119:18)"
    at async Promise.all (index 0)
    at async TestGroupTest.run (http://localhost:8080/out/suites/unittests/test_group_test.js:6:5)
    at async RunCaseSpecific.fn (http://localhost:8080/out/suites/unittests/test_group.spec.js:53:15)
    at async RunCaseSpecific.run (http://localhost:8080/out/framework/test_group.js:119:7)
    at async runCase (http://localhost:8080/out/runtime/standalone.js:37:17)
    at async http://localhost:8080/out/runtime/standalone.js:102:7`,
  },
  {
    case: 'multiple_lines',
    _expectedLines: 3,
    _stack: `Error: hello
    at RunCaseSpecific.fn (http://localhost:8080/out/suites/unittests/test_group.spec.js:48:11)
    at RunCaseSpecific.fn (http://localhost:8080/out/suites/unittests/test_group.spec.js:48:11)
    at RunCaseSpecific.fn (http://localhost:8080/out/suites/unittests/test_group.spec.js:48:11)
    at RunCaseSpecific.run (http://localhost:8080/out/framework/test_group.js:119:18)"
    at async Promise.all (index 0)
    at async TestGroupTest.run (http://localhost:8080/out/suites/unittests/test_group_test.js:6:5)
    at async RunCaseSpecific.fn (http://localhost:8080/out/suites/unittests/test_group.spec.js:53:15)
    at async RunCaseSpecific.run (http://localhost:8080/out/framework/test_group.js:119:7)
    at async runCase (http://localhost:8080/out/runtime/standalone.js:37:17)
    at async http://localhost:8080/out/runtime/standalone.js:102:7`,
  },
]);
