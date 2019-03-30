import { getGPU } from "../framework/gpu/implementation.js";
import {
  GPUBuffer,
  GPUDevice,
  GPUQueue,
} from "../framework/gpu/interface.js";
import { CaseRecorder, Fixture, FixtureCreate, IParamsAny } from "../framework/index.js";
import Shaderc from "../../third_party/shaderc/shaderc.js"

interface GPUTestOpts {
  device: any;
  shaderc: any;
}

export function makeGPUTestCreate<FC extends typeof GPUTest, F extends GPUTest>(fixture: FC): FixtureCreate<F> {
  return async (log: CaseRecorder, params: IParamsAny) => {
    const gpu = await getGPU();
    const adapter = await gpu.requestAdapter();
    const device = await adapter.requestDevice({});
    const shaderc = await Shaderc;
    return new fixture(log, params, {device, shaderc}) as F;
  };
}

export class GPUTest extends Fixture {
  public static create = makeGPUTestCreate(GPUTest);

  //public device: GPUDevice;
  public device: any; // TODO: update framework/gpu/ to match sketch again
  public queue: GPUQueue;
  public shaderc: any;

  public constructor(log: CaseRecorder, params: IParamsAny, opts: GPUTestOpts) {
    super(log, params);
    this.device = opts.device;
    this.queue = this.device.getQueue();
    this.shaderc = opts.shaderc;
  }

  public compile(type: ("f" | "v" | "c"), source: string): ArrayBuffer {
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
    return result.GetBinary().slice().buffer;
  }

  public expect(success: boolean, message: string): void {
    if (!success) {
      this.rec.fail(message);
    }
  }

  public async expectContents(src: GPUBuffer, expected: Uint8Array): Promise<void> {
    const size = expected.length;
    const dst = this.device.createBuffer({
      size: expected.length,
      usage: 1 | 8,
    });

    const c = this.device.createCommandEncoder({});
    c.copyBufferToBuffer(src, 0, dst, 0, size);

    this.queue.submit([c.finish()]);

    const ab = await dst.mapReadAsync();
    const actual = new Uint8Array(ab);
    for (let i = 0; i < size; ++i) {
      if (actual[i] !== expected[i]) {
        this.rec.fail(`at [${i}], expected ${expected[i]}, got ${actual[i]}`);
        // TODO: limit number of fail logs for one expectContents?
      }
    }
    // TODO: log the actual and expected data
  }
}
