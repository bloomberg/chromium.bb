export const description = `
Manual tests for pass/fail display and output behavior.
`;

import {
  DefaultFixture,
  TestGroup,
} from '../framework/index.js';

export const group = new TestGroup();

group.test('test_pass', null, DefaultFixture, (t) => {
  t.log('hello');
});

group.test('test_warn', null, DefaultFixture, (t) => {
  t.warn();
});

group.test('test_fail', null, DefaultFixture, (t) => {
  t.fail();
});
