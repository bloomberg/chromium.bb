export const name = "unittests";
export const description = `
Unit tests for CTS framework.
`;
export const subtrees = [
  import("./logger"),
  import("./params"),
  import("./test_tree_filesystem"),
];
