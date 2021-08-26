export const description = `
Stress tests covering robustness in the presence of non-halting shaders.
`;

import { makeTestGroup } from '../../common/framework/test_group.js';
import { GPUTest } from '../../webgpu/gpu_test.js';

export const g = makeTestGroup(GPUTest);

g.test('compute')
  .desc(`Tests execution of compute passes with non-halting dispatch operations.`)
  .unimplemented();

g.test('vertex')
  .desc(`Tests execution of render passes with a non-halting vertex stage.`)
  .unimplemented();

g.test('fragment')
  .desc(`Tests execution of render passes with a non-halting fragment stage.`)
  .unimplemented();
