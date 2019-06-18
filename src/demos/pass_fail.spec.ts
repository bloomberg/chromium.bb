export const description = `
Manual tests for pass/fail display and output behavior.
`;

import {
  DefaultFixture,
  TestGroup,
} from '../framework/index.js';

export const group = new TestGroup();

group.test('test_pass', DefaultFixture, (t) => {
  t.log('hello');
});

group.test('test_warn', DefaultFixture, (t) => {
  t.warn();
});

group.test('test_fail', DefaultFixture, (t) => {
  t.fail();
});
