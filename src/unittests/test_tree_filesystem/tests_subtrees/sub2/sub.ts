export const name = "sub";
export const description = ``;

import {
  TestTree,
} from "framework";

export function add(tree: TestTree) {
  tree.test("test1", () => {
    console.log("THIS_LINE_WAS_HIT:452a6787-ba57-4d43-a02e-6201875afb0d");
  });
  tree.test("test2", () => {
    console.log("THIS_LINE_WAS_HIT:4829aefc-5908-4439-88cf-bb5fc5d7a9fe");
  });
}
