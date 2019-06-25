export const description = `
Manual tests for pass/fail display and output behavior.
`;

import { DefaultFixture, TestGroup } from '../../framework/index.js';

export const g = new TestGroup(DefaultFixture);

g.test('test_pass', t => {
  t.log('hello');
});

g.test('test_warn', t => {
  t.warn();
});

g.test('test_fail', t => {
  t.fail();
});
