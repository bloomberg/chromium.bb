export const name = "sub";
export const description = ``;

import {
  TestTree,
} from "framework";

export function add(tree: TestTree) {
  tree.test("test1", () => {
    console.log("THIS_LINE_WAS_HIT:6d73faad-ca77-426a-8dc6-900cca1523eb");
  });
  tree.test("test2", () => {
    console.log("THIS_LINE_WAS_HIT:bf0cd187-6ec7-4f8b-b6c8-7acd27db00ee");
  });
}
