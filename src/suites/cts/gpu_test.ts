import { getGPU } from '../../framework/gpu/implementation.js';
import { Fixture, assert, unreachable } from '../../framework/index.js';

type glslang = typeof import('@webgpu/glslang/dist/web-devel/glslang');
type Glslang = import('@webgpu/glslang/dist/web-devel/glslang').Glslang;
type ShaderStage = import('@webgpu/glslang/dist/web-devel/glslang').ShaderStage;

let glslangInstance: Glslang | undefined;

class DevicePool {
  device: GPUDevice | undefined = undefined;
  state: 'free' | 'acquired' | 'uninitialized' | 'failed' = 'uninitialized';

  private async initialize(): Promise<void> {
    try {
      const gpu = getGPU();
      const adapter = await gpu.requestAdapter();
      this.device = await adapter.requestDevice();
    } catch (ex) {
      this.state = 'failed';
      throw ex;
    }
  }

  async acquire(): Promise<GPUDevice> {
    assert(this.state !== 'acquired', 'Device was in use');
    assert(this.state !== 'failed', 'Failed to initialize WebGPU device');

    const state = this.state;
    this.state = 'acquired';
    if (state === 'uninitialized') {
      await this.initialize();
    }

    assert(!!this.device);
    return this.device;
  }

  release(device: GPUDevice): void {
    assert(this.state === 'acquired');
    assert(device === this.device, 'Released device was the wrong device');
    this.state = 'free';
  }
}

const devicePool = new DevicePool();

export class GPUTest extends Fixture {
  device: GPUDevice = undefined!;
  queue: GPUQueue = undefined!;
  initialized = false;
  private supportsSPIRV = true;

  async init(): Promise<void> {
    await super.init();

    this.device = await devicePool.acquire();
    this.queue = this.device.defaultQueue;

    const isSafari = /^((?!chrome|android).)*safari/i.test(navigator.userAgent);
    if (isSafari) {
      this.supportsSPIRV = false;
    }

    try {
      await this.device.popErrorScope();
      unreachable('There was an error scope on the stack at the beginning of the test');
    } catch (ex) {}

    this.device.pushErrorScope('out-of-memory');
    this.device.pushErrorScope('validation');

    this.initialized = true;
  }

  async finalize(): Promise<void> {
    await super.finalize();

    if (this.initialized) {
      const gpuValidationError = await this.device.popErrorScope();
      if (gpuValidationError !== null) {
        assert(gpuValidationError instanceof GPUValidationError);
        this.fail(`Unexpected validation error occurred: ${gpuValidationError.message}`);
      }

      const gpuOutOfMemoryError = await this.device.popErrorScope();
      if (gpuOutOfMemoryError !== null) {
        assert(gpuOutOfMemoryError instanceof GPUOutOfMemoryError);
        this.fail('Unexpected out-of-memory error occurred');
      }
    }

    if (this.device) {
      devicePool.release(this.device);
    }
  }

  async initGLSL(): Promise<void> {
    if (!glslangInstance) {
      const glslangPath = '../../glslang.js';
      let glslangModule: () => Promise<Glslang>;
      try {
        glslangModule = ((await import(glslangPath)) as glslang).default;
      } catch (ex) {
        this.skip('glslang is not available');
      }
      await new Promise(resolve => {
        glslangModule().then((glslang: Glslang) => {
          glslangInstance = glslang;
          resolve();
        });
      });
    }
  }

  createShaderModule(desc: GPUShaderModuleDescriptor): GPUShaderModule {
    if (!this.supportsSPIRV) {
      this.skip('SPIR-V not available');
    }
    return this.device.createShaderModule(desc);
  }

  makeShaderModuleFromGLSL(stage: ShaderStage, glsl: string): GPUShaderModule {
    assert(
      glslangInstance !== undefined,
      'GLSL compiler is not instantiated. Run `await t.initGLSL()` first'
    );

    const code = glslangInstance.compileGLSL(glsl, stage, false);
    return this.device.createShaderModule({ code });
  }

  createCopyForMapRead(src: GPUBuffer, size: number): GPUBuffer {
    const dst = this.device.createBuffer({
      size,
      usage: GPUBufferUsage.MAP_READ | GPUBufferUsage.COPY_DST,
    });

    const c = this.device.createCommandEncoder();
    c.copyBufferToBuffer(src, 0, dst, 0, size);

    this.queue.submit([c.finish()]);

    return dst;
  }

  // TODO: add an expectContents for textures, which logs data: uris on failure

  expectContents(src: GPUBuffer, expected: ArrayBufferView): void {
    const exp = new Uint8Array(expected.buffer, expected.byteOffset, expected.byteLength);
    const dst = this.createCopyForMapRead(src, expected.buffer.byteLength);

    this.eventualAsyncExpectation(async niceStack => {
      const actual = new Uint8Array(await dst.mapReadAsync());
      const check = this.checkBuffer(actual, exp);
      if (check !== undefined) {
        niceStack.message = check;
        this.rec.fail(niceStack);
      }
      dst.destroy();
    });
  }

  expectBuffer(actual: Uint8Array, exp: Uint8Array): void {
    const check = this.checkBuffer(actual, exp);
    if (check !== undefined) {
      this.rec.fail(new Error(check));
    }
  }

  checkBuffer(actual: Uint8Array, exp: Uint8Array): string | undefined {
    const size = exp.byteLength;
    if (actual.byteLength !== size) {
      return 'size mismatch';
    }
    const lines = [];
    let failedPixels = 0;
    for (let i = 0; i < size; ++i) {
      if (actual[i] !== exp[i]) {
        if (failedPixels > 4) {
          lines.push('... and more');
          break;
        }
        failedPixels++;
        lines.push(`at [${i}], expected ${exp[i]}, got ${actual[i]}`);
      }
    }

    // TODO: Could make a more convenient message, which could look like e.g.:
    //
    //   Starting at offset 48,
    //              got 22222222 ABCDABCD 99999999
    //     but expected 22222222 55555555 99999999
    //
    // or
    //
    //   Starting at offset 0,
    //              got 00000000 00000000 00000000 00000000 (... more)
    //     but expected 00FF00FF 00FF00FF 00FF00FF 00FF00FF (... more)
    //
    // Or, maybe these diffs aren't actually very useful (given we have the prints just above here),
    // and we should remove them. More important will be logging of texture data in a visual format.

    if (size <= 256 && failedPixels > 0) {
      const expHex = Array.from(exp)
        .map(x => x.toString(16).padStart(2, '0'))
        .join('');
      const actHex = Array.from(actual)
        .map(x => x.toString(16).padStart(2, '0'))
        .join('');
      lines.push('EXPECT: ' + expHex);
      lines.push('ACTUAL: ' + actHex);
    }
    if (failedPixels) {
      return lines.join('\n');
    }
    return undefined;
  }
}
