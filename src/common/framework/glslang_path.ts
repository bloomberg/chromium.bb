import { assert } from './util/util.js';

let glslangPath: string | undefined;

export function getGlslangPath(): string | undefined {
  return glslangPath;
}

export function setGlslangPath(path: string): void {
  assert(path.startsWith('/'), 'glslang path must be absolute');
  glslangPath = path;
}
