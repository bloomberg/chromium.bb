export const description = `
Manual tests for tests which throw/reject.
`;

import { DefaultFixture, TestGroup } from '../../framework/index.js';

export const group = new TestGroup(DefaultFixture);

group.test('sync_throw', null, t => {
  throw new Error();
});

group.test('async_throw', null, async t => {
  throw new Error();
});

group.test('promise_reject', null, t => {
  return Promise.reject(new Error());
});
