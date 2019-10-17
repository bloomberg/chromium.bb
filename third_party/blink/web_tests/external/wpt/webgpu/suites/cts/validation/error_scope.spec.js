/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/

function _defineProperty(obj, key, value) { if (key in obj) { Object.defineProperty(obj, key, { value: value, enumerable: true, configurable: true, writable: true }); } else { obj[key] = value; } return obj; }

export const description = `
error scope validation tests.
`;
import { getGPU } from '../../../framework/gpu/implementation.js';
import { Fixture, TestGroup, rejectOnTimeout } from '../../../framework/index.js';

class F extends Fixture {
  constructor(...args) {
    super(...args);

    _defineProperty(this, "device", undefined);
  }

  async init() {
    super.init();
    const gpu = getGPU();
    const adapter = await gpu.requestAdapter();
    this.device = await adapter.requestDevice();
  }

  createErrorBuffer() {
    this.device.createBuffer({
      size: 1024,
      usage: 0xffff // Invalid GPUBufferUsage

    }); // TODO: Remove when chrome does it automatically.

    this.device.getQueue().submit([]);
  }

  async expectUncapturedError(fn) {
    return this.asyncExpectation(() => {
      // TODO: Make arbitrary timeout value a test runner variable
      const TIMEOUT_IN_MS = 1000;
      const promise = new Promise(resolve => {
        const eventListener = event => {
          this.debug(`Got uncaptured error event with ${event.error}`);
          resolve(event);
        };

        this.device.addEventListener('uncapturederror', eventListener, {
          once: true
        });
      });
      fn();
      return Promise.race([promise, rejectOnTimeout(TIMEOUT_IN_MS, 'Timeout occurred waiting for uncaptured error')]);
    });
  }

}

export const g = new TestGroup(F);
g.test('simple case where the error scope catches an error', async t => {
  t.device.pushErrorScope('validation');
  t.createErrorBuffer();
  const error = await t.device.popErrorScope();
  t.expect(error instanceof GPUValidationError);
});
g.test('errors bubble to the parent scope if not handled by the current scope', async t => {
  t.device.pushErrorScope('validation');
  t.device.pushErrorScope('out-of-memory');
  t.createErrorBuffer();
  {
    const error = await t.device.popErrorScope();
    t.expect(error === null);
  }
  {
    const error = await t.device.popErrorScope();
    t.expect(error instanceof GPUValidationError);
  }
});
g.test('if an error scope matches an error it does not bubble to the parent scope', async t => {
  t.device.pushErrorScope('validation');
  t.device.pushErrorScope('validation');
  t.createErrorBuffer();
  {
    const error = await t.device.popErrorScope();
    t.expect(error instanceof GPUValidationError);
  }
  {
    const error = await t.device.popErrorScope();
    t.expect(error === null);
  }
});
g.test('if no error scope handles an error it fires an uncapturederror event', async t => {
  t.device.pushErrorScope('out-of-memory');
  const uncapturedErrorEvent = await t.expectUncapturedError(() => {
    t.createErrorBuffer();
  });
  t.expect(uncapturedErrorEvent.error instanceof GPUValidationError);
  const error = await t.device.popErrorScope();
  t.expect(error === null);
});
g.test('push/popping sibling error scopes must be balanced', async t => {
  {
    const promise = t.device.popErrorScope();
    await t.shouldReject('OperationError', promise);
  }
  const promises = [];

  for (let i = 0; i < 1000; i++) {
    t.device.pushErrorScope('validation');
    promises.push(t.device.popErrorScope());
  }

  const errors = await Promise.all(promises);
  t.expect(errors.every(e => e === null));
  {
    const promise = t.device.popErrorScope();
    await t.shouldReject('OperationError', promise);
  }
});
g.test('push/popping nested error scopes must be balanced', async t => {
  {
    const promise = t.device.popErrorScope();
    await t.shouldReject('OperationError', promise);
  }
  const promises = [];

  for (let i = 0; i < 1000; i++) {
    t.device.pushErrorScope('validation');
  }

  for (let i = 0; i < 1000; i++) {
    promises.push(t.device.popErrorScope());
  }

  const errors = await Promise.all(promises);
  t.expect(errors.every(e => e === null));
  {
    const promise = t.device.popErrorScope();
    await t.shouldReject('OperationError', promise);
  }
});
//# sourceMappingURL=error_scope.spec.js.map