import { assert, unreachable } from './util/util.js';

type glslang = typeof import('@webgpu/glslang/dist/web-devel/glslang');
type Glslang = import('@webgpu/glslang/dist/web-devel/glslang').Glslang;
type ShaderStage = import('@webgpu/glslang/dist/web-devel/glslang').ShaderStage;
type SpirvVersion = import('@webgpu/glslang/dist/web-devel/glslang').SpirvVersion;

let glslangAttempted: boolean = false;
let glslangInstance: Glslang | undefined;

export async function initGLSL(): Promise<void> {
  if (glslangAttempted) {
    assert(glslangInstance !== undefined, 'glslang is not available');
  } else {
    glslangAttempted = true;
    const glslangPath = '../../third_party/glslang_js/lib/glslang.js';
    let glslangModule: () => Promise<Glslang>;
    try {
      glslangModule = ((await import(glslangPath)) as glslang).default;
    } catch (ex) {
      unreachable('glslang is not available');
    }

    const glslang = await glslangModule();
    glslangInstance = glslang;
  }
}

export function compileGLSL(
  glsl: string,
  shaderType: ShaderStage,
  genDebug: boolean,
  spirvVersion?: SpirvVersion
): Uint32Array {
  assert(
    glslangInstance !== undefined,
    'GLSL compiler is not instantiated. Run `await initGLSL()` first'
  );
  return glslangInstance.compileGLSL(glsl.trimLeft(), shaderType, genDebug, spirvVersion);
}
