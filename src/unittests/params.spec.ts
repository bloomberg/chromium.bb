export const description = `
Unit tests for parameterization system.
`;

import {
  CaseRecorder,
  Test,
  pcombine,
  poptions,
} from "../framework/index.js";

export const test = new Test();

test.case("test_fail", (log) => {
  log.fail();
});

test.case("test_sync", (log) => {
});
test.case("test_async", async (log) => {
});
test.caseP("ptest_sync", [{}], (log, p) => {
  log.log(JSON.stringify(p));
});
test.caseP("ptest_async", [{}], async (log, p) => {
  log.log(JSON.stringify(p));
});

function print(log: CaseRecorder, p: object) {
  log.log(JSON.stringify(p));
}

test.caseP("literal", [{hello: 1}, {hello: 2}], print);
test.caseP("list", poptions("hello", [1, 2, 3]), print);
test.caseP("combine_none", pcombine([]), print);
test.caseP("combine_unit_unit", pcombine([[{}], [{}]]), print);
test.caseP("combine_lists", pcombine([
  poptions("x", [1, 2]),
  poptions("y", ["a", "b"]),
  [{}],
]), print);
test.caseP("combine_arrays", pcombine([
  [{x: 1, y: 2}, {x: 10, y: 20}],
  [{z: "z"}, {w: "w"}],
]), print);
test.caseP("combine_mixed", pcombine([
  poptions("x", [1, 2]),
  [{z: "z"}, {w: "w"}],
]), print);
