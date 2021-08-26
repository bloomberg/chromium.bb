export const description = `
Stress tests covering GPURenderPassEncoder usage.
`;

import { makeTestGroup } from '../../common/framework/test_group.js';
import { GPUTest } from '../../webgpu/gpu_test.js';

export const g = makeTestGroup(GPUTest);

g.test('many')
  .desc(
    `Tests execution of a huge number of render passes using the same
GPURenderPipeline.`
  )
  .unimplemented();

g.test('pipeline_churn')
  .desc(
    `Tests execution of a huge number of render passes which each use a different
GPURenderPipeline.`
  )
  .unimplemented();

g.test('bind_group_churn')
  .desc(
    `Tests execution of compute passes which switch between a huge number of bind
groups.`
  )
  .unimplemented();

g.test('many_draws')
  .desc(`Tests execution of render passes with a huge number of draw calls`)
  .unimplemented();

g.test('huge_draws').desc(`Tests execution of render passes with huge draw calls`).unimplemented();
