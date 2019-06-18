export const description = `
mapWriteAsync tests.
`;

import { poptions, TestGroup } from '../../framework/index.js';
import { GPUTest } from '../gpu_test.js';

export const group = new TestGroup();

for (const params of poptions('value', [0x00000001, 0x01020304])) {
  group.test('basic', GPUTest, async (t) => {
    const value = t.params.value;

    const buf = t.device.createBuffer({ size: 12, usage: 2 | 4 });
    const mappedBuffer = new Uint32Array(await buf.mapWriteAsync());
    mappedBuffer[1] = value;
    buf.unmap();
    t.expect(mappedBuffer.length === 0, 'Mapped buffer should be detached.');
    await t.expectContents(buf, new Uint8Array(new Uint32Array([0, value, 0]).buffer));
  }, params);
}
