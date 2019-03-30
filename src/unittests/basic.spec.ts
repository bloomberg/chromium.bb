export const description = `
Basic unit tests for test framework.
`;

import {
  TestGroup,
} from "../framework/index.js";
import { UnitTest } from "./unit_test.js";

export const group = new TestGroup();

group.test("test_sync", UnitTest, (t) => {
});

group.test("test_async", UnitTest, async (t) => {
});

group.testp("testp_sync", {}, UnitTest, (t) => {
  t.log(JSON.stringify(t.params));
});

group.testp("testp_async", {}, UnitTest, async (t) => {
  t.log(JSON.stringify(t.params));
});
