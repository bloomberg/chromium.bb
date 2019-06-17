import * as Shaderc from '@webgpu/shaderc';
import { getGPU } from '../framework/gpu/implementation.js';
import { Fixture } from '../framework/index.js';

let shaderc: Promise<Shaderc.Shaderc> | undefined;

export class GPUTest extends Fixture {
  public device: GPUDevice = undefined as any;
  public queue: GPUQueue = undefined as any;
  public shaderc: Shaderc.Shaderc = undefined as any;

  public async init(): Promise<void> {
    super.init();
    const gpu = await getGPU();
    const adapter = await gpu.requestAdapter();
    this.device = await adapter.requestDevice({});
    this.queue = this.device.getQueue();

    shaderc = shaderc || Shaderc.instantiate();
    this.shaderc = await shaderc;
  }

  public makeShaderModule(type: ('f' | 'v' | 'c'), source: string): GPUShaderModule {
    return this.device.createShaderModule({ code: this.compile(type, source) });
  }

  public expect(success: boolean, message: string): void {
    if (!success) {
      this.rec.fail(message);
    }
  }

  public async expectContents(src: GPUBuffer, expected: ArrayBufferView): Promise<void> {
    const exp = new Uint8Array(expected.buffer, expected.byteOffset, expected.byteLength);

    const size = expected.buffer.byteLength;
    const dst = this.device.createBuffer({
      size: expected.buffer.byteLength,
      usage: 1 | 8,
    });

    const c = this.device.createCommandEncoder({});
    c.copyBufferToBuffer(src, 0, dst, 0, size);

    this.queue.submit([c.finish()]);

    const actual = new Uint8Array(await dst.mapReadAsync());
    for (let i = 0; i < size; ++i) {
      if (actual[i] !== exp[i]) {
        this.rec.fail(`at [${i}], expected ${exp[i]}, got ${actual[i]}`);
        // TODO: limit number of fail logs for one expectContents?
      }
    }
    // TODO: log the actual and expected data
  }

  private compile(type: ('f' | 'v' | 'c'), source: string): Uint32Array {
    const compiler = new this.shaderc.Compiler();
    const opts = new this.shaderc.CompileOptions();
    const result = compiler.CompileGlslToSpv(source,
        type === 'f' ? this.shaderc.shader_kind.fragment :
        type === 'v' ? this.shaderc.shader_kind.vertex :
        this.shaderc.shader_kind.compute,
        'a.glsl', 'main', opts);
    const error = result.GetErrorMessage();
    if (error) {
      // tslint:disable-next-line: no-console
      console.warn(error);
    }
    return result.GetBinary();
  }
}
