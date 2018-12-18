export const name = "sub2";
export const description = ``;

export const subtrees = [
  import('./sub'),
];

import {
  TestTree,
} from "framework";

export function add(tree: TestTree) {
  tree.test("test1", () => {
    console.log("THIS_LINE_WAS_HIT:38442515-c553-4dc6-9cfb-03e0c82cda33");
  });
  tree.test("test2", () => {
    console.log("THIS_LINE_WAS_HIT:34e4ea2b-5115-438b-a52e-d32cbbe3b6c3");
  });
}
