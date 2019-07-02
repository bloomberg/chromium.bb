/// <reference types="@webgpu/types" />

let impl: GPU | undefined = undefined;

export function getGPU(): GPU {
  if (impl) {
    return impl;
  }

  if (typeof navigator === 'undefined' || navigator.gpu === undefined) {
    throw new Error('No WebGPU implementation found');
  }

  impl = navigator.gpu;
  return impl;
}
