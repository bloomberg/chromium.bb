export const name = "sub1";
export const description = ``;

export const subtrees = [
  import('./sub'),
];

import {
  TestTree,
} from "framework";

export function add(tree: TestTree) {
  tree.test("test1", () => {
    console.log("THIS_LINE_WAS_HIT:73d58cf6-bbbb-401b-b686-612e89cd9967");
  });
  tree.test("test2", () => {
    console.log("THIS_LINE_WAS_HIT:915d4cc4-1a20-4c2c-abfe-630cc033108c");
  });
}
