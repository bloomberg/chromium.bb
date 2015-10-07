(function(global, binding, v8) {
  'use strict';

  const defineProperty = global.Object.defineProperty;

  class ByteLengthQueuingStrategy {
    constructor(options) {
      defineProperty(this, 'highWaterMark', {
        value: options.highWaterMark,
        enumerable: true,
        configurable: true,
        writable: true
      });
    }
    size(chunk) { return chunk.byteLength; }
  }

  defineProperty(global, 'ByteLengthQueuingStrategy', {
    value: ByteLengthQueuingStrategy,
    enumerable: false,
    configurable: true,
    writable: true
  });
});
