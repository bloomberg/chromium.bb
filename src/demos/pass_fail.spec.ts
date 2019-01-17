export const description = `
Basic unit tests for test framework.
`;

import {
  TestGroup,
} from "../framework/index.js";

export const group = new TestGroup();

group.test("test_pass", (t) => {
  t.log("hello");
});

group.test("test_warn", (t) => {
  t.warn();
});

group.test("test_fail", (t) => {
  t.fail();
});
