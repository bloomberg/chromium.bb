export const description =
  'Test out-of-memory conditions creating large mappable/mappedAtCreation buffers.';

import { poptions, params, pbool } from '../../../../common/framework/params_builder.js';
import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { kBufferUsages } from '../../../capability_info.js';
import { GPUTest } from '../../../gpu_test.js';

// A multiple of 8 guaranteed to be way too large to allocate (just under 8 pebibytes).
// (Note this is likely to exceed limitations other than just the system's
// physical memory - so test cases are also needed to try to trigger "true" OOM.)
const MAX_ALIGNED_SAFE_INTEGER = Number.MAX_SAFE_INTEGER - 7;

const oomAndSizeParams = params()
  .combine(pbool('oom'))
  .expand(({ oom }) => {
    if (oom) {
      return poptions('size', [
        MAX_ALIGNED_SAFE_INTEGER,
        0x2000000000, // 128 GB
      ]);
    } else {
      return poptions('size', [16]);
    }
  });

export const g = makeTestGroup(GPUTest);

g.test('mapAsync')
  .desc(
    `Test creating a large mappable buffer should produce an out-of-memory error if allocation fails.
  - The resulting buffer is an error buffer, so mapAsync rejects and produces a validation error.
  - Calling getMappedRange should throw an OperationError because the buffer is not in the mapped state.
  - unmap() throws an OperationError if mapping failed, and otherwise should detach the ArrayBuffer.
`
  )
  .cases(oomAndSizeParams)
  .subcases(() => params().combine(pbool('write')))
  .fn(async t => {
    const { oom, write, size } = t.params;

    const buffer = t.expectGPUError(
      'out-of-memory',
      () =>
        t.device.createBuffer({
          size,
          usage: write ? GPUBufferUsage.MAP_WRITE : GPUBufferUsage.MAP_READ,
        }),
      oom
    );
    const promise = t.expectGPUError(
      'validation', // Should be a validation error since the buffer is invalid.
      () => buffer.mapAsync(write ? GPUMapMode.WRITE : GPUMapMode.READ),
      oom
    );

    if (oom) {
      // Should also reject in addition to the validation error.
      t.shouldReject('OperationError', promise);

      // Should throw an OperationError because the buffer is not mapped.
      // Note: not a RangeError because the state of the buffer is checked first.
      t.shouldThrow('OperationError', () => {
        buffer.getMappedRange();
      });

      // Should throw an OperationError because the buffer is already unmapped.
      t.shouldThrow('OperationError', () => {
        buffer.unmap();
      });
    } else {
      await promise;
      const arraybuffer = buffer.getMappedRange();
      t.expect(arraybuffer.byteLength === size);
      buffer.unmap();
      t.expect(arraybuffer.byteLength === 0, 'Mapping should be detached');
    }
  });

g.test('mappedAtCreation,full_getMappedRange')
  .desc(
    `Test creating a very large buffer mappedAtCreation buffer should produce
an out-of-memory error if allocation fails.
  - Because the buffer can be immediately mapped, getMappedRange does not throw an OperationError. It throws a RangeError because such a
    large ArrayBuffer cannot be created.
  - unmap() should not throw.
  `
  )
  .cases(oomAndSizeParams)
  .subcases(() => params().combine(poptions('usage', kBufferUsages)))
  .fn(async t => {
    const { oom, usage, size } = t.params;

    const buffer = t.expectGPUError(
      'out-of-memory',
      () => t.device.createBuffer({ mappedAtCreation: true, size, usage }),
      oom
    );

    const f = () => buffer.getMappedRange();

    let mapping: ArrayBuffer | undefined = undefined;
    if (oom) {
      t.shouldThrow('RangeError', f);
    } else {
      mapping = f();
    }
    buffer.unmap();
    if (mapping !== undefined) {
      t.expect(mapping.byteLength === 0, 'Mapping should be detached');
    }
  });

g.test('mappedAtCreation,smaller_getMappedRange')
  .desc(
    `Test creating a very large mappedAtCreation buffer should produce
an out-of-memory error if allocation fails.
  - Because the buffer can be immediately mapped, getMappedRange does not throw an OperationError. Calling it on a small range of the buffer successfully returns an ArrayBuffer.
  - unmap() should detach the ArrayBuffer.
  `
  )
  .cases(oomAndSizeParams)
  .subcases(() => poptions('usage', kBufferUsages))
  .fn(async t => {
    const { usage, size } = t.params;

    const buffer = t.expectGPUError('out-of-memory', () =>
      t.device.createBuffer({ mappedAtCreation: true, size, usage })
    );

    // Smaller range inside a too-big mapping
    const mapping = buffer.getMappedRange(0, 16);
    t.expect(mapping.byteLength === 16);
    buffer.unmap();
    t.expect(mapping.byteLength === 0, 'Mapping should be detached');
  });
