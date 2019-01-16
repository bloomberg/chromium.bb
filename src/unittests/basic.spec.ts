export const description = `
Basic unit tests for test framework.
`;

import {
  TestGroup,
} from "../framework/index.js";

export const group = new TestGroup();

group.test("test_sync", function() {
});

group.test("test_async", async function() {
});

group.testp("testp_sync", {}, function() {
  this.log(JSON.stringify(this.params));
});

group.testp("testp_async", {}, async function() {
  this.log(JSON.stringify(this.params));
});
