export const description = `
TODO:

- Start a pipeline statistics query in all possible encoders:
    - queryIndex {in, out of} range for GPUQuerySet
    - GPUQuerySet {valid, invalid}
    - x ={render pass, compute pass} encoder
`;

import { params, poptions } from '../../../../../common/framework/params_builder.js';
import { makeTestGroup } from '../../../../../common/framework/test_group.js';
import { kQueryTypes } from '../../../../capability_info.js';
import { ValidationTest } from '../../validation_test.js';

import { createQuerySetWithType, createRenderEncoderWithQuerySet } from './common.js';

export const g = makeTestGroup(ValidationTest);

g.test('occlusion_query,query_type')
  .desc(
    `
Tests that set occlusion query set with all types in render pass descriptor:
- type {occlusion (control case), pipeline statistics, timestamp}
- {undefined} for occlusion query set in render pass descriptor
  `
  )
  .subcases(() => poptions('type', [undefined, ...kQueryTypes]))
  .fn(async t => {
    const type = t.params.type;

    if (type) {
      await t.selectDeviceForQueryTypeOrSkipTestCase(type);
    }

    const querySet = type === undefined ? undefined : createQuerySetWithType(t, type, 1);

    const encoder = createRenderEncoderWithQuerySet(t, querySet);
    encoder.encoder.beginOcclusionQuery(0);
    encoder.encoder.endOcclusionQuery();

    t.expectValidationError(() => {
      encoder.finish();
    }, type !== 'occlusion');
  });

g.test('occlusion_query,invalid_query_set')
  .desc(
    `
Tests that begin occlusion query with a invalid query set that failed during creation.
  `
  )
  .subcases(() => poptions('querySetState', ['valid', 'invalid'] as const))
  .fn(t => {
    const querySet = t.createQuerySetWithState(t.params.querySetState);

    const encoder = createRenderEncoderWithQuerySet(t, querySet);
    encoder.encoder.beginOcclusionQuery(0);
    encoder.encoder.endOcclusionQuery();

    t.expectValidationError(() => {
      encoder.finish();
    }, t.params.querySetState === 'invalid');
  });

g.test('occlusion_query,query_index')
  .desc(
    `
Tests that begin occlusion query with query index:
- queryIndex {in, out of} range for GPUQuerySet
  `
  )
  .subcases(() => poptions('queryIndex', [0, 2]))
  .fn(t => {
    const querySet = createQuerySetWithType(t, 'occlusion', 2);

    const encoder = createRenderEncoderWithQuerySet(t, querySet);
    encoder.encoder.beginOcclusionQuery(t.params.queryIndex);
    encoder.encoder.endOcclusionQuery();

    t.expectValidationError(() => {
      encoder.finish();
    }, t.params.queryIndex > 0);
  });

g.test('timestamp_query,query_type_and_index')
  .desc(
    `
Tests that write timestamp to all types of query set on all possible encoders:
- type {occlusion, pipeline statistics, timestamp}
- queryIndex {in, out of} range for GPUQuerySet
- x= {non-pass, compute, render} encoder
  `
  )
  .cases(
    params()
      .combine(poptions('encoderType', ['non-pass', 'compute pass', 'render pass'] as const))
      .combine(poptions('type', kQueryTypes))
  )
  .subcases(({ type }) => poptions('queryIndex', type === 'timestamp' ? [0, 2] : [0]))
  .fn(async t => {
    const { encoderType, type, queryIndex } = t.params;

    await t.selectDeviceForQueryTypeOrSkipTestCase(type);

    const count = 2;
    const querySet = createQuerySetWithType(t, type, count);

    const encoder = t.createEncoder(encoderType);
    encoder.encoder.writeTimestamp(querySet, queryIndex);

    t.expectValidationError(() => {
      encoder.finish();
    }, type !== 'timestamp' || queryIndex >= count);
  });

g.test('timestamp_query,invalid_query_set')
  .desc(
    `
Tests that write timestamp to a invalid query set that failed during creation:
- x= {non-pass, compute, render} enconder
  `
  )
  .subcases(() => poptions('encoderType', ['non-pass', 'compute pass', 'render pass'] as const))
  .fn(async t => {
    const querySet = t.createQuerySetWithState('invalid');

    const encoder = t.createEncoder(t.params.encoderType);
    encoder.encoder.writeTimestamp(querySet, 0);

    t.expectValidationError(() => {
      encoder.finish();
    });
  });
