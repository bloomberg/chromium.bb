export const description = `
Basic tests.
`;

import { TestGroup } from '../../framework/index.js';
import { GPUTest } from '../gpu_test.js';

export const group = new TestGroup(GPUTest);

group.test('empty', null, async t => {
  const encoder = t.device.createCommandEncoder({});
  const cmd = encoder.finish();
  t.device.getQueue().submit([cmd]);

  // TODO: test that submit() succeeded.
});
