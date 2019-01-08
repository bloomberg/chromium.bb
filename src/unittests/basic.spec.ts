export const description = `
Basic unit tests for test framework.
`;

import {
  TestGroup,
} from "../framework/index.js";

export const group = new TestGroup();

// These are intended to test the display of failing tests.
// TODO: These tests should be skipped, once there is a mechanism to do so.
group.test("test_fail", {}, (log) => {
  log.fail();
});

group.test("test_warn", {}, (log) => {
  log.warn();
});

group.test("test_sync", {}, (log) => {
});

group.test("test_async", {}, async (log) => {
});

group.test("test_params_sync", {
  cases: [{}],
}, (log, p) => {
  log.log(JSON.stringify(p));
});

group.test("ptest_params_async", {
  cases: [{}],
}, async (log, p) => {
  log.log(JSON.stringify(p));
});
