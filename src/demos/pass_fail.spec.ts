export const description = `
Manual tests for pass/fail display and output behavior.
`;

import { DefaultFixture, TestGroup } from '../framework/index.js';

export const group = new TestGroup(DefaultFixture);

group.test('test_pass', null, t => {
  t.log('hello');
});

group.test('test_warn', null, t => {
  t.warn();
});

group.test('test_fail', null, t => {
  t.fail();
});
