export const name = "test_tree_filesystem";
export const description = `
Unit tests for TestTree's filesystem structure.
`;

export const subtrees = [
  import("./notests_nosubtrees"),
  import("./tests_nosubtrees"),
  import("./tests_subtrees"),
];
