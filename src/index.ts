import { TestTree } from "framework";

(async () => {
  const trunk = await TestTree.trunk([
    import("./unittests"),
    import("./conformance"),
  ]);

  // Print test listing
  for await (const t of trunk.iterate()) {
  }

  // tslint:disable-next-line:no-console
  console.log("");

  // Actually run tests
  for await (const t of trunk.iterate()) {
    t();
  }
})();
