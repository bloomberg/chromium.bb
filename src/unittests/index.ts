export const name = "unittests";
export const description = `
Unit tests for namespaced logging system.

Also serves as a larger test of async test functions, and of the logging system.
`;
export const subtrees = [
  import("./logger"),
  import("./params"),
  import("./test_tree_filesystem"),
];
