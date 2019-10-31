import { getGPU } from '../../framework/gpu/implementation.js';
import { Fixture } from '../../framework/index.js';

type glslang = typeof import('@webgpu/glslang/dist/web-devel/glslang');
type Glslang = import('@webgpu/glslang/dist/web-devel/glslang').Glslang;
type ShaderStage = import('@webgpu/glslang/dist/web-devel/glslang').ShaderStage;

let glslangInstance: Glslang | undefined;

// TODO: Should this gain some functionality currently only in UnitTest?
export class GPUTest extends Fixture {
  device: GPUDevice = undefined!;
  queue: GPUQueue = undefined!;
  initialized = false;

  async init(): Promise<void> {
    super.init();
    const gpu = getGPU();
    const adapter = await gpu.requestAdapter();
    this.device = await adapter.requestDevice();
    this.queue = this.device.getQueue();

    try {
      await this.device.popErrorScope();
      throw new Error('There was an error scope on the stack at the beginning of the test');
    } catch (ex) {}

    this.device.pushErrorScope('out-of-memory');
    this.device.pushErrorScope('validation');

    this.initialized = true;
  }

  async finalize(): Promise<void> {
    super.finalize();

    if (this.initialized) {
      const gpuValidationError = await this.device.popErrorScope();
      if (gpuValidationError !== null) {
        if (!(gpuValidationError instanceof GPUValidationError)) throw new Error();
        this.fail(`Unexpected validation error occurred: ${gpuValidationError.message}`);
      }

      const gpuOutOfMemoryError = await this.device.popErrorScope();
      if (gpuOutOfMemoryError !== null) {
        if (!(gpuOutOfMemoryError instanceof GPUOutOfMemoryError)) throw new Error();
        this.fail('Unexpected out-of-memory error occurred');
      }
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

  makeShaderModule(stage: ShaderStage, source: string): GPUShaderModule {
    if (!glslangInstance) {
      throw new Error('GLSL is not instantiated. Run `await t.initGLSL()` first');
    }
    const code = glslangInstance.compileGLSL(source, stage, false);
    return this.device.createShaderModule({ code });
  }

  // TODO: add an expectContents for textures, which logs data: uris on failure

  expectContents(src: GPUBuffer, expected: ArrayBufferView): void {
    const exp = new Uint8Array(expected.buffer, expected.byteOffset, expected.byteLength);

    const size = expected.buffer.byteLength;
    const dst = this.device.createBuffer({
      size: expected.buffer.byteLength,
      usage: GPUBufferUsage.MAP_READ | GPUBufferUsage.COPY_DST,
    });

    const c = this.device.createCommandEncoder();
    c.copyBufferToBuffer(src, 0, dst, 0, size);

    this.queue.submit([c.finish()]);

    this.eventualAsyncExpectation(async () => {
      const actual = new Uint8Array(await dst.mapReadAsync());
      this.expectBuffer(actual, exp);
      dst.destroy();
    });
  }

  expectBuffer(actual: Uint8Array, exp: Uint8Array): void {
    const size = exp.byteLength;
    if (actual.byteLength !== size) {
      this.rec.fail('size mismatch');
      return;
    }
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
      this.rec.log('EXPECT: ' + expHex);
      this.rec.log('ACTUAL: ' + actHex);
    }
  }
}
