/// <reference types="@webgpu/types" />

let impl: Promise<GPU>;

export function getGPU(): Promise<GPU> {
  if (impl) {
    return impl;
  }

  if (typeof navigator !== 'undefined' && 'gpu' in navigator) {
    impl = Promise.resolve(navigator.gpu as GPU);
  } else {
    // tslint:disable-next-line no-console
    console.warn('Neither navigator.gpu nor Dawn was found. Using dummy.');
    impl = import('./dummy.js').then(mod => mod.default);
  }
  return impl;
}
