export const description = `
Basic unit tests for test framework.
`;

import {
  TestGroup,
} from "../framework/index.js";

export const group = new TestGroup();

group.test("test_sync", (t) => {
});

group.test("test_async", async (t) => {
});

group.testp("testp_sync", {}, (t) => {
  t.log(JSON.stringify(t.params));
});

group.testp("testp_async", {}, async (t) => {
  t.log(JSON.stringify(t.params));
});
