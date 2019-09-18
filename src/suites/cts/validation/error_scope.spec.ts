export const description = `
error scope validation tests.
`;

import { getGPU } from '../../../framework/gpu/implementation.js';
import { Fixture, TestGroup } from '../../../framework/index.js';

function rejectTimeout(ms: number, msg: string): Promise<void> {
  return new Promise((resolve, reject) => {
    setTimeout(() => {
      reject(new Error(msg));
    }, ms);
  });
}

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
    this.device.getQueue().submit([]);
  }

  async expectUncapturedError(fn: Function): Promise<void> {
    // TODO: Make arbitrary timeout value a test runner variable
    const TIMEOUT_IN_MS = 1000;

    return this.asyncExpectation(async () => {
      try {
        const promise = new Promise(resolve => {
          this.device.addEventListener('uncapturederror', resolve, { once: true });
        });

        fn();

        await Promise.race([
          promise,
          rejectTimeout(TIMEOUT_IN_MS, 'Uncaptured error timeout occurred'),
        ]);
      } catch (error) {
        this.fail(error.message);
      }
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

  await t.expectUncapturedError(() => {
    t.createErrorBuffer();
  });

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
