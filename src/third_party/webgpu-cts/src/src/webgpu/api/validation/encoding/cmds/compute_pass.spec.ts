export const description = `
API validation test for compute pass

Does **not** test usage scopes (resource_usages/) or programmable pass stuff (programmable_pass).
`;

import { makeTestGroup } from '../../../../../common/framework/test_group.js';
import { DefaultLimits } from '../../../../constants.js';
import { kResourceStates, ResourceState, ValidationTest } from '../../validation_test.js';

class F extends ValidationTest {
  createComputePipeline(state: 'valid' | 'invalid'): GPUComputePipeline {
    if (state === 'valid') {
      return this.createNoOpComputePipeline();
    }

    return this.createErrorComputePipeline();
  }

  createIndirectBuffer(state: ResourceState, data: Uint32Array): GPUBuffer {
    const descriptor: GPUBufferDescriptor = {
      size: data.byteLength,
      usage: GPUBufferUsage.INDIRECT | GPUBufferUsage.COPY_DST,
    };

    if (state === 'invalid') {
      descriptor.usage = 0xffff; // Invalid GPUBufferUsage
    }

    this.device.pushErrorScope('validation');
    const buffer = this.device.createBuffer(descriptor);
    this.device.popErrorScope();

    if (state === 'valid') {
      this.queue.writeBuffer(buffer, 0, data);
    }

    if (state === 'destroyed') {
      buffer.destroy();
    }

    return buffer;
  }
}

export const g = makeTestGroup(F);

g.test('set_pipeline')
  .desc(
    `
setPipeline should generate an error iff using an 'invalid' pipeline.
`
  )
  .params(u => u.beginSubcases().combine('state', ['valid', 'invalid'] as const))
  .fn(t => {
    const { state } = t.params;
    const pipeline = t.createComputePipeline(state);

    const { encoder, validateFinishAndSubmitGivenState } = t.createEncoder('compute pass');
    encoder.setPipeline(pipeline);
    validateFinishAndSubmitGivenState(state);
  });

g.test('pipeline,device_mismatch')
  .desc('Tests setPipeline cannot be called with a compute pipeline created from another device')
  .paramsSubcasesOnly(u => u.combine('mismatched', [true, false]))
  .unimplemented();

const kMaxDispatch = DefaultLimits.maxComputeWorkgroupsPerDimension;
g.test('dispatch_sizes')
  .desc(
    `Test 'direct' and 'indirect' dispatch with various sizes.

  Only direct dispatches can produce validation errors.
  Workgroup sizes:
    - valid: { zero, one, just under limit }
    - invalid: { just over limit, way over limit }

  TODO: Verify that the invalid cases don't execute any invocations at all.
`
  )
  .params(u =>
    u
      .combine('dispatchType', ['direct', 'indirect'] as const)
      .combine('largeDimValue', [0, 1, kMaxDispatch, kMaxDispatch + 1, 0x7fff_ffff, 0xffff_ffff])
      .beginSubcases()
      .combine('largeDimIndex', [0, 1, 2] as const)
      .combine('smallDimValue', [0, 1])
  )
  .fn(t => {
    const { dispatchType, largeDimIndex, smallDimValue, largeDimValue } = t.params;

    const pipeline = t.createNoOpComputePipeline();

    const workSizes = [smallDimValue, smallDimValue, smallDimValue];
    workSizes[largeDimIndex] = largeDimValue;

    const { encoder, validateFinishAndSubmit } = t.createEncoder('compute pass');
    encoder.setPipeline(pipeline);
    if (dispatchType === 'direct') {
      const [x, y, z] = workSizes;
      encoder.dispatch(x, y, z);
    } else if (dispatchType === 'indirect') {
      encoder.dispatchIndirect(t.createIndirectBuffer('valid', new Uint32Array(workSizes)), 0);
    }

    const shouldError =
      dispatchType === 'direct' &&
      (workSizes[0] > kMaxDispatch || workSizes[1] > kMaxDispatch || workSizes[2] > kMaxDispatch);

    validateFinishAndSubmit(!shouldError, true);
  });

const kBufferData = new Uint32Array(6).fill(1);
g.test('indirect_dispatch_buffer')
  .desc(
    `
Test dispatchIndirect validation by submitting various dispatches with a no-op pipeline and an
indirectBuffer with 6 elements.
- indirectBuffer: {'valid', 'invalid', 'destroyed'}
- indirectOffset:
  - valid, within the buffer: {beginning, middle, end} of the buffer
  - invalid, non-multiple of 4
  - invalid, the last element is outside the buffer

TODO: test specifically which call the validation error occurs in.
      (Should be finish() for invalid, but submit() for destroyed.)
`
  )
  .paramsSubcasesOnly(u =>
    u //
      .combine('state', kResourceStates)
      .combine('offset', [
        // valid (for 'valid' buffers)
        0,
        Uint32Array.BYTES_PER_ELEMENT,
        kBufferData.byteLength - 3 * Uint32Array.BYTES_PER_ELEMENT,
        // invalid, non-multiple of 4 offset
        1,
        // invalid, last element outside buffer
        kBufferData.byteLength - 2 * Uint32Array.BYTES_PER_ELEMENT,
      ])
  )
  .fn(t => {
    const { state, offset } = t.params;
    const pipeline = t.createNoOpComputePipeline();
    const buffer = t.createIndirectBuffer(state, kBufferData);

    const { encoder, validateFinishAndSubmit } = t.createEncoder('compute pass');
    encoder.setPipeline(pipeline);
    encoder.dispatchIndirect(buffer, offset);

    const finishShouldError =
      state === 'invalid' ||
      offset % 4 !== 0 ||
      offset + 3 * Uint32Array.BYTES_PER_ELEMENT > kBufferData.byteLength;
    validateFinishAndSubmit(!finishShouldError, state !== 'destroyed');
  });

g.test('indirect_dispatch_buffer,device_mismatch')
  .desc(
    'Tests dispatchIndirect cannot be called with an indirect buffer created from another device'
  )
  .paramsSubcasesOnly(u => u.combine('mismatched', [true, false]))
  .unimplemented();
