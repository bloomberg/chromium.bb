export const description = `
Stress tests for GPUAdapter.requestDevice.
`;

import { Fixture } from '../../common/framework/fixture.js';
import { makeTestGroup } from '../../common/framework/test_group.js';

export const g = makeTestGroup(Fixture);

g.test('coexisting').desc(`Tests allocation of many coexisting GPUDevice objects.`).unimplemented();

g.test('continuous,with_destroy')
  .desc(
    `Tests allocation and destruction of many GPUDevice objects over time. Objects
are sequentially requested and destroyed over a very large number of
iterations.`
  )
  .unimplemented();

g.test('continuous,no_destroy')
  .desc(
    `Tests allocation and implicit GC of many GPUDevice objects over time. Objects
are sequentially requested and dropped for GC over a very large number of
iterations.`
  )
  .unimplemented();
