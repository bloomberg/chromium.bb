type ShaderStage = import('@webgpu/glslang').ShaderStage;

// tslint:disable-next-line: no-default-export
export default function GLSL(stage: ShaderStage, source: string): Uint32Array {
  throw new Error('macro stub implementation');
}
