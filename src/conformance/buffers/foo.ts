export const name = "foo";
export const description = `
Foo.
`;

import { TestTree } from "framework";

export function add(tree: TestTree) {
  tree.test("bar", async () => {
    //
  });
}
