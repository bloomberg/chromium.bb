import { TestTree } from "framework";

(async () => {
  const trunk = await TestTree.trunk([
    import("./unittests"),
    import("./conformance"),
  ]);
  const it = trunk.run();
  for await (const x of trunk.run()) {
  }
})();
