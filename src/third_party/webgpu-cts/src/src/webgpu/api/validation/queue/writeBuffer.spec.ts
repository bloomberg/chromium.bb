export const description = `
Tests writeBuffer validation.

Note: destroyed buffer is tested in destroyed/.
Note: buffer map state is tested in ./buffer_mapped.spec.ts.
`;

import { makeTestGroup } from '../../../../common/framework/test_group.js';
import {
  TypedArrayBufferView,
  TypedArrayBufferViewConstructor,
} from '../../../../common/util/util.js';
import { GPUConst } from '../../../constants.js';
import { ValidationTest } from '../validation_test.js';

export const g = makeTestGroup(ValidationTest);

g.test('ranges')
  .desc(
    `
Tests that the data ranges given to GPUQueue.writeBuffer() are properly validated. Tests calling
writeBuffer with both TypedArrays and ArrayBuffers and checks that the data offset and size is
interpreted correctly for both.

  - When passing a TypedArray the data offset and size is given in elements.
  - When passing an ArrayBuffer the data offset and size is given in bytes.

Also verifies that the specified data range:

  - Describes a valid range of the destination buffer and source buffer.
  - Fits fully within the destination buffer.
  - Has a byte size which is a multiple of 4.
`
  )
  .fn(async t => {
    const queue = t.device.queue;

    function runTest(arrayType: TypedArrayBufferViewConstructor, testBuffer: boolean) {
      const elementSize = arrayType.BYTES_PER_ELEMENT;
      const bufferSize = 16 * elementSize;
      const buffer = t.device.createBuffer({
        size: bufferSize,
        usage: GPUBufferUsage.COPY_DST,
      });
      const arraySm: TypedArrayBufferView | ArrayBuffer = testBuffer
        ? new arrayType(8).buffer
        : new arrayType(8);
      const arrayMd: TypedArrayBufferView | ArrayBuffer = testBuffer
        ? new arrayType(16).buffer
        : new arrayType(16);
      const arrayLg: TypedArrayBufferView | ArrayBuffer = testBuffer
        ? new arrayType(32).buffer
        : new arrayType(32);

      if (elementSize < 4) {
        const array15: TypedArrayBufferView | ArrayBuffer = testBuffer
          ? new arrayType(15).buffer
          : new arrayType(15);

        // Writing the full buffer that isn't 4-byte aligned.
        t.shouldThrow('OperationError', () => queue.writeBuffer(buffer, 0, array15));

        // Writing from an offset that causes source to be 4-byte aligned.
        queue.writeBuffer(buffer, 0, array15, 3);

        // Writing from an offset that causes the source to not be 4-byte aligned.
        t.shouldThrow('OperationError', () => queue.writeBuffer(buffer, 0, arrayMd, 3));

        // Writing with a size that is not 4-byte aligned.
        t.shouldThrow('OperationError', () => queue.writeBuffer(buffer, 0, arraySm, 0, 7));
      }

      // Writing the full buffer without offsets.
      queue.writeBuffer(buffer, 0, arraySm);
      queue.writeBuffer(buffer, 0, arrayMd);
      t.expectValidationError(() => queue.writeBuffer(buffer, 0, arrayLg));

      // Writing the full buffer with a 4-byte aligned offset.
      queue.writeBuffer(buffer, 8, arraySm);
      t.expectValidationError(() => queue.writeBuffer(buffer, 8, arrayMd));

      // Writing the full buffer with a unaligned offset.
      t.shouldThrow('OperationError', () => queue.writeBuffer(buffer, 3, arraySm));

      // Writing remainder of buffer from offset.
      queue.writeBuffer(buffer, 0, arraySm, 4);
      queue.writeBuffer(buffer, 0, arrayMd, 4);
      t.expectValidationError(() => queue.writeBuffer(buffer, 0, arrayLg, 4));

      // Writing a larger buffer from an offset that allows it to fit in the destination.
      queue.writeBuffer(buffer, 0, arrayLg, 16);

      // Writing with both an offset and size.
      queue.writeBuffer(buffer, 0, arraySm, 4, 4);

      // Writing with a size that extends past the source buffer length.
      t.shouldThrow('OperationError', () => queue.writeBuffer(buffer, 0, arraySm, 0, 16));
      t.shouldThrow('OperationError', () => queue.writeBuffer(buffer, 0, arraySm, 4, 8));

      // Writing with a size that is 4-byte aligned but an offset that is not.
      queue.writeBuffer(buffer, 0, arraySm, 3, 4);

      // Writing zero bytes at the end of the buffer
      queue.writeBuffer(buffer, bufferSize, arraySm, 0, 0);

      // Writing with a buffer offset that is out of range of buffer size
      t.expectValidationError(() => queue.writeBuffer(buffer, bufferSize + 4, arraySm, 0, 0));

      // Writing zero bytes from the end of the data
      queue.writeBuffer(buffer, 0, arraySm, 8, 0);

      // Writing with a data offset that is out of range of data size
      t.shouldThrow('OperationError', () => queue.writeBuffer(buffer, 0, arraySm, 9, 0));

      // A data offset of undefined should be treated as 0
      queue.writeBuffer(buffer, 0, arraySm, undefined, 8);
      t.shouldThrow('OperationError', () => queue.writeBuffer(buffer, 0, arraySm, undefined, 12));
    }

    const arrayTypes = [
      Uint8Array,
      Uint8ClampedArray,
      Int8Array,
      Uint16Array,
      Int16Array,
      Uint32Array,
      Int32Array,
      Float32Array,
      Float64Array,
    ];

    runTest(Uint8Array, true);

    for (const arrayType of arrayTypes) {
      runTest(arrayType, false);
    }
  });

g.test('usages')
  .desc(
    `
Tests calling writeBuffer with the buffer missed COPY_DST usage.
- buffer {with, without} COPY DST usage
`
  )
  .paramsSubcasesOnly([
    { usage: GPUConst.BufferUsage.COPY_DST, _valid: true }, // control case
    { usage: GPUConst.BufferUsage.STORAGE, _valid: false }, // without COPY_DST usage
    { usage: GPUConst.BufferUsage.STORAGE | GPUConst.BufferUsage.COPY_SRC, _valid: false }, // with other usage
    { usage: GPUConst.BufferUsage.STORAGE | GPUConst.BufferUsage.COPY_DST, _valid: true }, // with COPY_DST usage
  ])
  .fn(async t => {
    const { usage, _valid } = t.params;
    const buffer = t.device.createBuffer({ size: 16, usage });
    const data = new Uint8Array(16);

    t.expectValidationError(() => {
      t.device.queue.writeBuffer(buffer, 0, data, 0, data.length);
    }, !_valid);
  });

g.test('buffer,device_mismatch')
  .desc('Tests writeBuffer cannot be called with a buffer created from another device')
  .paramsSubcasesOnly(u => u.combine('mismatched', [true, false]))
  .unimplemented();
