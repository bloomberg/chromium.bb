export const description = `
Stress tests covering vertex buffer usage.
`;

import { makeTestGroup } from '../../common/framework/test_group.js';
import { GPUTest } from '../../webgpu/gpu_test.js';

export const g = makeTestGroup(GPUTest);

g.test('huge')
  .desc(
    `Tests execution of draw calls using as many huge vertex buffers as are
supported by the device, with as many attributes as are supported.`
  )
  .unimplemented();
