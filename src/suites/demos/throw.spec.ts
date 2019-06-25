export const description = `
Manual tests for tests which throw/reject.
`;

import { DefaultFixture, TestGroup } from '../../framework/index.js';

export const g = new TestGroup(DefaultFixture);

g.test('sync_throw', t => {
  throw new Error();
});

g.test('async_throw', async t => {
  throw new Error();
});

g.test('promise_reject', t => {
  return Promise.reject(new Error());
});
