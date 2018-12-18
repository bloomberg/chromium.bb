export const name = "tests_nosubtrees";
export const description = `
TestTree test: with tests, without subtrees
`;

import {
  TestTree,
} from "framework";

export function add(tree: TestTree) {
  tree.test("test1", () => {
    console.log("THIS_LINE_WAS_HIT:69078bb3-72c8-4d17-9937-aef9158bfef9");
  });
  tree.test("test2", () => {
    console.log("THIS_LINE_WAS_HIT:b7782904-3770-49f3-99c0-f461b557eea6");
  });
}
