import { getGPU } from "../framework/gpu/implementation.js";
import {
  GPUBuffer,
  GPUQueue,
  GPUShaderModule,
} from "../framework/gpu/interface.js";
import { CaseRecorder, Fixture, FixtureCreate, IParamsAny } from "../framework/index.js";
import * as Shaderc from '@webgpu/shaderc';

interface GPUTestOpts {
  // TODO: update framework/gpu/ to match sketch again
  //device: GPUDevice;
  device: any;
  shaderc: Shaderc.Shaderc;
}

const shaderc: Shaderc.Shaderc = Shaderc.instantiate();

export function makeGPUTestCreate<FC extends typeof GPUTest, F extends GPUTest>(fixture: FC): FixtureCreate<F> {
  return async (log: CaseRecorder, params: IParamsAny) => {
    const gpu = await getGPU();
    const adapter = await gpu.requestAdapter();
    const device = await adapter.requestDevice({});
    return new fixture(log, params, {device, shaderc: await shaderc}) as F;
  };
}

export class GPUTest extends Fixture {
  public static create = makeGPUTestCreate(GPUTest);

  public device: any; // TODO: type
  public queue: GPUQueue;
  public shaderc: any; // TODO: type

  public constructor(log: CaseRecorder, params: IParamsAny, opts: GPUTestOpts) {
    super(log, params);
    this.device = opts.device;
    this.queue = this.device.getQueue();
    this.shaderc = opts.shaderc;
  }

  private compile(type: ("f" | "v" | "c"), source: string): ArrayBuffer {
    const compiler = new this.shaderc.Compiler();
    const opts = new this.shaderc.CompileOptions();
    const result = compiler.CompileGlslToSpv(source,
        type === "f" ? this.shaderc.shader_kind.fragment :
        type === "v" ? this.shaderc.shader_kind.vertex :
        type === "c" ? this.shaderc.shader_kind.compute : null,
        "a.glsl", "main", opts);
    const error = result.GetErrorMessage();
    if (error) {
      console.warn(error);
    }
    return result.GetBinary();
  }

  public makeShaderModule(type: ("f" | "v" | "c"), source: string): GPUShaderModule {
    return this.device.createShaderModule({ code: this.compile(type, source) });
  }

  public expect(success: boolean, message: string): void {
    if (!success) {
      this.rec.fail(message);
    }
  }

  public async expectContents(src: GPUBuffer, expected: ArrayBufferView): Promise<void> {
    const size = expected.buffer.byteLength;
    const dst = this.device.createBuffer({
      size: expected.buffer.byteLength,
      usage: 1 | 8,
    });

    const c = this.device.createCommandEncoder({});
    c.copyBufferToBuffer(src, 0, dst, 0, size);

    this.queue.submit([c.finish()]);

    const actual = new expected.constructor(await dst.mapReadAsync());
    for (let i = 0; i < size; ++i) {
      if (actual[i] !== expected[i]) {
        this.rec.fail(`at [${i}], expected ${expected[i]}, got ${actual[i]}`);
        // TODO: limit number of fail logs for one expectContents?
      }
    }
    // TODO: log the actual and expected data
  }
}
