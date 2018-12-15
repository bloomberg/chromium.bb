import { TestTree } from 'framework'

(async () => {
  const trunk = await TestTree.trunk([
    import('./unittests'),
    import('./conformance'),
  ]);
  for await (const x of trunk.run()) {
  }
})();
