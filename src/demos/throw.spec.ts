export const description = `
Manual tests for tests which throw/reject.
`;

import {
  TestGroup,
} from "../framework/index.js";

export const group = new TestGroup();

group.test("sync_throw", (t) => {
  throw new Error();
});

group.test("async_throw", async (t) => {
  throw new Error();
});

group.test("promise_reject", (t) => {
  return Promise.reject(new Error());
});
