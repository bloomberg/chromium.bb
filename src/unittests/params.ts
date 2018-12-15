export const name = "params";
export const description = `
Unit tests for parameterization system.
`;

import {
  ParamIterable,
  pcombine,
  poptions,
  punit,
  TestTree,
} from "framework";

export function add(tree: TestTree) {
  tree.test("test_sync", () => {
  });
  tree.test("test_async", async () => {
  });
  tree.ptest("ptest_sync", punit(), (p: object) => {
    // tslint:disable-next-line:no-console
    console.log(p);
  });
  tree.ptest("ptest_async", punit(), async (p: object) => {
    // tslint:disable-next-line:no-console
    console.log(p);
  });

  function ptestSimple(n: string, params: ParamIterable) {
    // tslint:disable-next-line:no-console
    tree.ptest(n, params, console.log);
  }

  ptestSimple("list", poptions("hello", [1, 2, 3]));
  ptestSimple("unit", punit());
  ptestSimple("combine_none", pcombine([]));
  ptestSimple("combine_unit_unit", pcombine([punit(), punit()]));
  ptestSimple("combine_lists", pcombine([
    poptions("x", [1, 2]),
    poptions("y", ["a", "b"]),
    punit(),
  ]));
  ptestSimple("combine_arrays", pcombine([
    [{x: 1, y: 2}, {x: 10, y: 20}],
    [{z: "z"}, {w: "w"}],
  ]));
  ptestSimple("combine_mixed", pcombine([
    poptions("x", [1, 2]),
    [{z: "z"}, {w: "w"}],
  ]));
}
