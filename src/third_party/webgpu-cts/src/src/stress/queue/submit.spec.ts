export const description = `
Stress tests for command submission to GPUQueue objects.
`;

import { makeTestGroup } from '../../common/framework/test_group.js';
import { GPUTest } from '../../webgpu/gpu_test.js';

export const g = makeTestGroup(GPUTest);

g.test('huge_command_buffer')
  .desc(
    `Tests submission of huge command buffers to a GPUQueue. Huge buffers are
encoded by chaining together repeated sequences of compute and copy
operations, with expected results verified at the end of the test.`
  )
  .unimplemented();

g.test('many_command_buffers')
  .desc(
    `Tests submission of a huge number of command buffers to a GPUQueue by a single
submit() call.`
  )
  .unimplemented();
