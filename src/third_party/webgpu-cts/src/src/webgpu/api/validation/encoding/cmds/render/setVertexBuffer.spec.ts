export const description = `
Validation tests for setVertexBuffer on render pass and render bundle.
`;

import { makeTestGroup } from '../../../../../../common/framework/test_group.js';
import { GPUConst, DefaultLimits } from '../../../../../constants.js';
import { ValidationTest } from '../../../validation_test.js';

import {
  kRenderEncodeTypeParams,
  kBufferStates,
  buildBufferOffsetAndSizeOOBTestParams,
} from './render.js';

export const g = makeTestGroup(ValidationTest);

g.test('slot')
  .desc(
    `
Tests slot must be less than the maxVertexBuffers in device limits.
  `
  )
  .paramsSubcasesOnly(
    kRenderEncodeTypeParams.combine('slot', [
      0,
      DefaultLimits.maxVertexBuffers - 1,
      DefaultLimits.maxVertexBuffers,
    ] as const)
  )
  .fn(t => {
    const { encoderType, slot } = t.params;
    const vertexBuffer = t.createBufferWithState('valid', {
      size: 16,
      usage: GPUBufferUsage.VERTEX,
    });

    const { encoder, finish } = t.createEncoder(encoderType);
    encoder.setVertexBuffer(slot, vertexBuffer);

    t.expectValidationError(() => {
      finish();
    }, slot >= DefaultLimits.maxVertexBuffers);
  });

g.test('vertex_buffer')
  .desc(
    `
Tests vertex buffer must be valid.
  `
  )
  .paramsSubcasesOnly(kRenderEncodeTypeParams.combine('state', kBufferStates))
  .fn(t => {
    const { encoderType, state } = t.params;
    const vertexBuffer = t.createBufferWithState(state, {
      size: 16,
      usage: GPUBufferUsage.VERTEX,
    });

    const { encoder, finish } = t.createEncoder(encoderType);
    encoder.setVertexBuffer(0, vertexBuffer);

    t.expectValidationError(() => {
      if (state === 'destroyed') {
        t.queue.submit([finish()]);
      } else {
        finish();
      }
    }, state !== 'valid');
  });

g.test('vertex_buffer_usage')
  .desc(
    `
Tests vertex buffer must have 'Vertex' usage.
  `
  )
  .paramsSubcasesOnly(
    kRenderEncodeTypeParams.combine('usage', [
      GPUConst.BufferUsage.VERTEX, // control case
      GPUConst.BufferUsage.COPY_DST,
      GPUConst.BufferUsage.COPY_DST | GPUConst.BufferUsage.VERTEX,
    ] as const)
  )
  .fn(t => {
    const { encoderType, usage } = t.params;
    const vertexBuffer = t.device.createBuffer({
      size: 16,
      usage,
    });

    const { encoder, finish } = t.createEncoder(encoderType);
    encoder.setVertexBuffer(0, vertexBuffer);

    t.expectValidationError(() => {
      finish();
    }, (usage | GPUConst.BufferUsage.VERTEX) !== usage);
  });

g.test('offset_alignment')
  .desc(
    `
Tests offset must be a multiple of 4.
  `
  )
  .paramsSubcasesOnly(kRenderEncodeTypeParams.combine('offset', [0, 2, 4] as const))
  .fn(t => {
    const { encoderType, offset } = t.params;
    const vertexBuffer = t.device.createBuffer({
      size: 16,
      usage: GPUBufferUsage.VERTEX,
    });

    const { encoder, finish } = t.createEncoder(encoderType);
    encoder.setVertexBuffer(0, vertexBuffer, offset);

    t.expectValidationError(() => {
      finish();
    }, offset % 4 !== 0);
  });

g.test('offset_and_size_oob')
  .desc(
    `
Tests offset and size cannot be larger than vertex buffer size.
  `
  )
  .paramsSubcasesOnly(buildBufferOffsetAndSizeOOBTestParams(4, 256))
  .fn(t => {
    const { encoderType, offset, size, _valid } = t.params;
    const vertexBuffer = t.device.createBuffer({
      size: 256,
      usage: GPUBufferUsage.VERTEX,
    });

    const { encoder, finish } = t.createEncoder(encoderType);
    encoder.setVertexBuffer(0, vertexBuffer, offset, size);

    t.expectValidationError(() => {
      finish();
    }, !_valid);
  });
