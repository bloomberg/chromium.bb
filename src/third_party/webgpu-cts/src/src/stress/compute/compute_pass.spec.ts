export const description = `
Stress tests covering GPUComputePassEncoder usage.
`;

import { makeTestGroup } from '../../common/framework/test_group.js';
import { GPUTest } from '../../webgpu/gpu_test.js';

export const g = makeTestGroup(GPUTest);

g.test('many')
  .desc(
    `Tests execution of a huge number of compute passes using the same
GPUComputePipeline.`
  )
  .unimplemented();

g.test('pipeline_churn')
  .desc(
    `Tests execution of a huge number of compute passes which each use a different
GPUComputePipeline.`
  )
  .unimplemented();

g.test('bind_group_churn')
  .desc(
    `Tests execution of compute passes which switch between a huge number of bind
groups.`
  )
  .unimplemented();

g.test('many_dispatches')
  .desc(`Tests execution of compute passes with a huge number of dispatch calls`)
  .unimplemented();

g.test('huge_dispatches')
  .desc(`Tests execution of compute passes with huge dispatch calls`)
  .unimplemented();
