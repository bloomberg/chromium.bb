import { TestTree } from "framework";

(async () => {
  const trunk = await TestTree.trunk([
    import("./unittests"),
  ]);

  // Print test listing
  for await (const {path, run} of trunk.iterate()) {
    console.log("*", path.join("/"));
  }

  // tslint:disable-next-line:no-console
  console.log("");

  // Actually run tests
  for await (const {path, run} of trunk.iterate()) {
    console.log("*", path.join("/"));
    run();
  }
})();
