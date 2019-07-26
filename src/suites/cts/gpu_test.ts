import { getGPU } from '../../framework/gpu/implementation.js';
import { Fixture } from '../../framework/index.js';

// TODO: Should this gain some functionality currently only in UnitTest?
export class GPUTest extends Fixture {
  device: GPUDevice = undefined!;
  queue: GPUQueue = undefined!;

  async init(): Promise<void> {
    super.init();
    const gpu = getGPU();
    const adapter = await gpu.requestAdapter();
    this.device = await adapter.requestDevice({});
    this.queue = this.device.getQueue();
  }

  expect(success: boolean, message: string): void {
    if (!success) {
      this.rec.fail(message);
    }
  }

  // TODO: add an expectContents for textures, which logs data: uris on failure

  async expectContents(src: GPUBuffer, expected: ArrayBufferView): Promise<void> {
    const exp = new Uint8Array(expected.buffer, expected.byteOffset, expected.byteLength);

    const size = expected.buffer.byteLength;
    const dst = this.device.createBuffer({
      size: expected.buffer.byteLength,
      usage: GPUBufferUsage.MAP_READ | GPUBufferUsage.COPY_DST,
    });

    const c = this.device.createCommandEncoder({});
    c.copyBufferToBuffer(src, 0, dst, 0, size);

    this.queue.submit([c.finish()]);

    const actual = new Uint8Array(await dst.mapReadAsync());
    let failedPixels = 0;
    for (let i = 0; i < size; ++i) {
      if (actual[i] !== exp[i]) {
        if (failedPixels > 4) {
          this.rec.fail('... and more');
          break;
        }
        failedPixels++;
        this.rec.fail(`at [${i}], expected ${exp[i]}, got ${actual[i]}`);
      }
    }
    if (size <= 256 && failedPixels > 0) {
      const expHex = Array.from(exp)
        .map(x => x.toString(16).padStart(2, '0'))
        .join('');
      const actHex = Array.from(actual)
        .map(x => x.toString(16).padStart(2, '0'))
        .join('');
      this.rec.log('EXP: ' + expHex);
      this.rec.log('ACT: ' + actHex);
    }
  }
}
