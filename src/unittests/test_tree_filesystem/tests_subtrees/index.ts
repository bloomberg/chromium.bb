export const name = "tests_subtrees";
export const description = `
TestTree test: with tests, with subtrees
`;

export const subtrees = [
  import("./sub1"),
  import("./sub2"),
];

import {
  TestTree,
} from "framework";

export function add(tree: TestTree) {
  tree.test("test1", () => {
    console.log("THIS_LINE_WAS_HIT:a25d4b04-b6c5-4fca-aff8-80ff632bf887");
  });
  tree.test("test2", () => {
    console.log("THIS_LINE_WAS_HIT:d194f5ea-c4b3-4eb4-a283-30690834cf22");
  });
}
