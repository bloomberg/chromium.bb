export const description = `
Manual tests for tests which throw/reject.
`;

import { DefaultFixture, TestGroup } from '../framework/index.js';

export const group = new TestGroup();

group.test('sync_throw', null, DefaultFixture, t => {
  throw new Error();
});

group.test('async_throw', null, DefaultFixture, async t => {
  throw new Error();
});

group.test('promise_reject', null, DefaultFixture, t => {
  return Promise.reject(new Error());
});
