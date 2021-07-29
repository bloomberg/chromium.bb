export const description = `
Tests using a destroyed query set on a queue.

TODO: Test with pipeline statistics queries on {compute, render} as well.
`;

import { makeTestGroup } from '../../../../../common/framework/test_group.js';
import { createRenderEncoderWithQuerySet } from '../../encoding/queries/common.js';
import { ValidationTest } from '../../validation_test.js';

export const g = makeTestGroup(ValidationTest);

g.test('beginOcclusionQuery')
  .desc(
    `
Tests that use a destroyed query set in occlusion query on render pass encoder.
- x= {destroyed, not destroyed (control case)}
  `
  )
  .paramsSubcasesOnly(u => u.combine('querySetState', ['valid', 'destroyed'] as const))
  .fn(t => {
    const querySet = t.createQuerySetWithState(t.params.querySetState);

    const encoder = createRenderEncoderWithQuerySet(t, querySet);
    encoder.encoder.beginOcclusionQuery(0);
    encoder.encoder.endOcclusionQuery();

    t.expectValidationError(() => {
      t.queue.submit([encoder.finish()]);
    }, t.params.querySetState === 'destroyed');
  });

g.test('writeTimestamp')
  .desc(
    `
Tests that use a destroyed query set in writeTimestamp on {non-pass, compute, render} encoder.
- x= {destroyed, not destroyed (control case)}
  `
  )
  .params(u =>
    u
      .combine('encoderType', ['non-pass', 'compute pass', 'render pass'] as const)
      .beginSubcases()
      .combine('querySetState', ['valid', 'destroyed'] as const)
  )
  .fn(async t => {
    await t.selectDeviceOrSkipTestCase('timestamp-query');

    const querySet = t.createQuerySetWithState(t.params.querySetState, {
      type: 'timestamp',
      count: 2,
    });

    const encoder = t.createEncoder(t.params.encoderType);
    encoder.encoder.writeTimestamp(querySet, 0);

    t.expectValidationError(() => {
      t.queue.submit([encoder.finish()]);
    }, t.params.querySetState === 'destroyed');
  });

g.test('resolveQuerySet')
  .desc(
    `
Tests that use a destroyed query set in resolveQuerySet.
- x= {destroyed, not destroyed (control case)}
  `
  )
  .paramsSubcasesOnly(u => u.combine('querySetState', ['valid', 'destroyed'] as const))
  .fn(async t => {
    const querySet = t.createQuerySetWithState(t.params.querySetState);

    const buffer = t.device.createBuffer({ size: 8, usage: GPUBufferUsage.QUERY_RESOLVE });

    const encoder = t.device.createCommandEncoder();
    encoder.resolveQuerySet(querySet, 0, 1, buffer, 0);

    t.expectValidationError(() => {
      t.queue.submit([encoder.finish()]);
    }, t.params.querySetState === 'destroyed');
  });
