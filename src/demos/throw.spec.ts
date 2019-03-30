export const description = `
Manual tests for tests which throw/reject.
`;

import {
  TestGroup, DefaultFixture,
} from "../framework/index.js";

export const group = new TestGroup();

group.test("sync_throw", DefaultFixture, (t) => {
  throw new Error();
});

group.test("async_throw", DefaultFixture, async (t) => {
  throw new Error();
});

group.test("promise_reject", DefaultFixture, (t) => {
  return Promise.reject(new Error());
});
