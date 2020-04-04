import { unreachable } from '../framework/util/util.js';

type ShaderStage = import('@webgpu/glslang').ShaderStage;

// tslint:disable-next-line: no-default-export
export default function GLSL(stage: ShaderStage, source: string): Uint32Array {
  unreachable('macro stub implementation');
}
