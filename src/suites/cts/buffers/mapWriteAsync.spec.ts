export const description = `
mapWriteAsync tests.
`;

import { TestGroup, poptions } from '../../../framework/index.js';
import { GPUTest } from '../gpu_test.js';

export const g = new TestGroup(GPUTest);

g.test('basic', async t => {
  const value = t.params.value;

  const buf = t.device.createBuffer({ size: 12, usage: 2 | 4 });
  const mappedBuffer = new Uint32Array(await buf.mapWriteAsync());
  mappedBuffer[1] = value;
  buf.unmap();
  await t.expectContents(buf, new Uint8Array(new Uint32Array([0, value, 0]).buffer));
}).params(poptions('value', [0x00000001, 0x01020304]));

g.test('unmap', async t => {
  const buf = t.device.createBuffer({ size: 12, usage: 2 | 4 });
  const mappedBuffer = await buf.mapWriteAsync();
  buf.unmap();
  t.expect(mappedBuffer.byteLength === 0, 'Mapped buffer should be detached.');
});
