export const description = `
Basic unit tests for test framework.
`;

import {
  TestGroup,
} from "../framework/index.js";

export const group = new TestGroup();

// These are intended to test the display of failing tests.
// TODO: These tests should be skipped, once there is a mechanism to do so.
group.test("test_fail", function() {
  this.fail();
});

group.test("test_warn", function() {
  this.warn();
});

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
