export const name = "buffers";
export const description = `
Buffer tests.
`;
export const subtrees = [
  import("./foo"),
];

import { TestTree } from "framework";

export function add(tree: TestTree) {
  tree.test("baz", async () => {
    //
  });
}
