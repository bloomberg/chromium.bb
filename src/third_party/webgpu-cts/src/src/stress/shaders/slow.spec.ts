export const description = `
Stress tests covering robustness in the presence of slow shaders.
`;

import { makeTestGroup } from '../../common/framework/test_group.js';
import { GPUTest } from '../../webgpu/gpu_test.js';

export const g = makeTestGroup(GPUTest);

g.test('compute')
  .desc(`Tests execution of compute passes with very long-running dispatch operations.`)
  .unimplemented();

g.test('vertex')
  .desc(`Tests execution of render passes with a very long-running vertex stage.`)
  .unimplemented();

g.test('fragment')
  .desc(`Tests execution of render passes with a very long-running fragment stage.`)
  .unimplemented();
