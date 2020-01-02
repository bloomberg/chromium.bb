/// <reference types="@webgpu/types" />

import { assert } from '../util/index.js';

let impl: GPU | undefined = undefined;

export function getGPU(): GPU {
  if (impl) {
    return impl;
  }

  assert(
    typeof navigator !== 'undefined' && navigator.gpu !== undefined,
    'No WebGPU implementation found'
  );

  impl = navigator.gpu;
  return impl;
}
