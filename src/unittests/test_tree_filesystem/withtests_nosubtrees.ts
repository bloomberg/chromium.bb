export const name = "withtests_nosubtrees";
export const description = `
TestTree test: with tests, without subtrees
`;

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
