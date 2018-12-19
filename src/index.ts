import { ConsoleLogger, TestTree } from "framework";

// tslint:disable:no-console
(async () => {
  const trunk = await TestTree.trunk([
    import("./unittests"),
  ]);

  const log = new ConsoleLogger();

  console.log("Test listing");
  for await (const t of trunk.iterate(log)) {
  }

  console.log("");
  console.log("Test results:");
  for await (const t of trunk.iterate(log)) {
    await t();
  }

  console.log("");
  console.log("Done.");
})();
