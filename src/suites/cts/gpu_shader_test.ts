import { GPUTest } from './gpu_test.js';

type glslang = typeof import('@webgpu/glslang');
type Glslang = import('@webgpu/glslang').Glslang;
type ShaderStage = import('@webgpu/glslang').ShaderStage;

let glslangInstance: Glslang = undefined!;

export class GPUShaderTest extends GPUTest {
  async init(): Promise<void> {
    await super.init();
    if (!glslangInstance) {
      const glslangPath = '../../glslang.js';
      const glslangModule = ((await import(glslangPath)) as glslang).default;
      await new Promise(resolve => {
        glslangModule().then((glslang: Glslang) => {
          glslangInstance = glslang;
          resolve();
        });
      });
    }
  }

  makeShaderModule(stage: ShaderStage, source: string): GPUShaderModule {
    return this.device.createShaderModule({ code: this.compile(stage, source) });
  }

  private compile(stage: ShaderStage, source: string): Uint32Array {
    const data = glslangInstance.compileGLSL(source, stage, false);
    return data;
  }
}
