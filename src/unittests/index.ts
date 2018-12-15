export const name = "unittests";
export const description = `
Unit tests for CTS framework.
`;
export const subtrees = [
  import("./params"),
  import("./test_tree"),
  import("./test_tree_filesystem"),
];
