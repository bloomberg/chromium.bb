export const description = `
API validation test for debug groups and markers

Test Coverage:
  - For each encoder type (GPUCommandEncoder, GPUComputeEncoder, GPURenderPassEncoder,
  GPURenderBundleEncoder):
    - Test that all pushDebugGroup must have a corresponding popDebugGroup
      - Push and pop counts of 0, 1, and 2 will be used.
      - An error must be generated for non matching counts.
    - Test calling pushDebugGroup with empty and non-empty strings.
    - Test inserting a debug marker with empty and non-empty strings.
`;

import { makeTestGroup } from '../../../../../common/framework/test_group.js';
import { ValidationTest, kEncoderTypes } from '../../validation_test.js';

export const g = makeTestGroup(ValidationTest);

g.test('debug_group_balanced')
  .params(u =>
    u
      .combine('encoderType', kEncoderTypes)
      .beginSubcases()
      .combine('pushCount', [0, 1, 2])
      .combine('popCount', [0, 1, 2])
  )
  .fn(t => {
    const { encoder, finish } = t.createEncoder(t.params.encoderType);
    for (let i = 0; i < t.params.pushCount; ++i) {
      encoder.pushDebugGroup(`${i}`);
    }
    for (let i = 0; i < t.params.popCount; ++i) {
      encoder.popDebugGroup();
    }
    const shouldError = t.params.popCount !== t.params.pushCount;
    t.expectValidationError(() => {
      const commandBuffer = finish();
      t.queue.submit([commandBuffer]);
    }, shouldError);
  });

g.test('debug_group')
  .params(u =>
    u //
      .combine('encoderType', kEncoderTypes)
      .beginSubcases()
      .combine('label', ['', 'group'])
  )
  .fn(t => {
    const { encoder, finish } = t.createEncoder(t.params.encoderType);
    encoder.pushDebugGroup(t.params.label);
    encoder.popDebugGroup();
    const commandBuffer = finish();
    t.queue.submit([commandBuffer]);
  });

g.test('debug_marker')
  .params(u =>
    u //
      .combine('encoderType', kEncoderTypes)
      .beginSubcases()
      .combine('label', ['', 'marker'])
  )
  .fn(t => {
    const maker = t.createEncoder(t.params.encoderType);
    maker.encoder.insertDebugMarker(t.params.label);
    const commandBuffer = maker.finish();
    t.queue.submit([commandBuffer]);
  });
