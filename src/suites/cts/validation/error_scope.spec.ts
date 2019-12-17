export const description = `
error scope validation tests.
`;

import { getGPU } from '../../../framework/gpu/implementation.js';
import { Fixture, TestGroup, raceWithRejectOnTimeout } from '../../../framework/index.js';

class F extends Fixture {
  device: GPUDevice = undefined!;

  async init(): Promise<void> {
    super.init();
    const gpu = getGPU();
    const adapter = await gpu.requestAdapter();
    this.device = await adapter.requestDevice();
  }

  createErrorBuffer(): void {
    this.device.createBuffer({
      size: 1024,
      usage: 0xffff, // Invalid GPUBufferUsage
    });
    // TODO: Remove when chrome does it automatically.
    this.device.defaultQueue.submit([]);
  }

  // Expect an uncapturederror event to occur. Note: this MUST be awaited, because
  // otherwise it could erroneously pass by capturing an error from later in the test.
  async expectUncapturedError(fn: Function): Promise<GPUUncapturedErrorEvent> {
    return this.immediateAsyncExpectation(() => {
      // TODO: Make arbitrary timeout value a test runner variable
      const TIMEOUT_IN_MS = 1000;

      const promise: Promise<GPUUncapturedErrorEvent> = new Promise(resolve => {
        const eventListener = ((event: GPUUncapturedErrorEvent) => {
          this.debug(`Got uncaptured error event with ${event.error}`);
          resolve(event);
        }) as EventListener;

        this.device.addEventListener('uncapturederror', eventListener, { once: true });
      });

      fn();

      return raceWithRejectOnTimeout(
        promise,
        TIMEOUT_IN_MS,
        'Timeout occurred waiting for uncaptured error'
      );
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
    t.shouldReject('OperationError', promise);
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
    t.shouldReject('OperationError', promise);
  }
});

g.test('push/popping nested error scopes must be balanced', async t => {
  {
    const promise = t.device.popErrorScope();
    t.shouldReject('OperationError', promise);
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
    t.shouldReject('OperationError', promise);
  }
});
