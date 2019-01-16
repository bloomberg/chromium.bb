export const description = `
Basic unit tests for test framework.
`;

import {
  TestGroup,
} from "../framework/index.js";

export const group = new TestGroup();

group.test("test_pass", function() {
  this.log("hello");
});

group.test("test_warn", function() {
  this.warn();
});

group.test("test_fail", function() {
  this.fail();
});
