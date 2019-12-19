export const description = ``;

import { TestGroup } from '../../../framework/index.js';
import { GPUTest } from '../gpu_test.js';

function getBufferDesc(): GPUBufferDescriptor {
  return {
    size: Number.MAX_SAFE_INTEGER,
    usage: GPUBufferUsage.MAP_WRITE,
  };
}

export const g = new TestGroup(GPUTest);

g.test('mapWriteAsync', async t => {
  const buffer = t.device.createBuffer(getBufferDesc());
  t.shouldReject('RangeError', buffer.mapWriteAsync());
});

g.test('mapReadAsync', async t => {
  const buffer = t.device.createBuffer(getBufferDesc());
  t.shouldReject('RangeError', buffer.mapReadAsync());
});

g.test('createBufferMapped', async t => {
  t.shouldThrow('RangeError', () => {
    t.device.createBufferMapped(getBufferDesc());
  });
});
