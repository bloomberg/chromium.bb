export const name = "sub2";
export const description = ``;
export const subtrees = [
  import("./sub"),
];

import {
  TestTree,
} from "framework";

export function add(tree: TestTree) {
  tree.test("test1", (log) => {
    log.expect(true);
  });
  tree.test("test2", (log) => {
    log.expect(true);
  });
}
