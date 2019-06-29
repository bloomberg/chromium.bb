/// <reference types="@webgpu/types" />

let impl: Promise<GPU>;

export function getGPU(): Promise<GPU> {
  if (impl) {
    return impl;
  }

  let dawn = false;
  if (typeof require !== 'undefined') {
    const fs = require('fs');
    if (fs.existsSync('dawn/index.node')) {
      dawn = true;
    }
  }

  if (typeof navigator !== 'undefined' && 'gpu' in navigator) {
    impl = Promise.resolve(navigator.gpu as GPU);
  } else if (dawn) {
    impl = import('../../../third_party/dawn').then(mod => mod.default);
  } else {
    // tslint:disable-next-line no-console
    console.warn('Neither navigator.gpu nor Dawn was found. Using dummy.');
    impl = import('./dummy.js').then(mod => mod.default);
  }
  return impl;
}
