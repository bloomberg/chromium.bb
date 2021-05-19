export const description = `
Tests for capability checking for features enabling optional query types.
`;

import { params, pbool, poptions } from '../../../../../common/framework/params_builder.js';
import { makeTestGroup } from '../../../../../common/framework/test_group.js';
import { ValidationTest } from '../../validation_test.js';

export const g = makeTestGroup(ValidationTest);

g.test('createQuerySet')
  .desc(
    `
Tests that creating query set shouldn't be valid without the required feature enabled.
- createQuerySet
  - type {occlusion, pipeline-statistics, timestamp}
  - x= {pipeline statistics, timestamp} query {enable, disable}

TODO: This test should expect *synchronous* exceptions, not validation errors, per
<https://github.com/gpuweb/gpuweb/blob/main/design/ErrorConventions.md>.
As of this writing, the spec needs to be fixed as well.
  `
  )
  .params(
    params()
      .combine(poptions('type', ['occlusion', 'pipeline-statistics', 'timestamp'] as const))
      .combine(pbool('pipelineStatisticsQueryEnable'))
      .combine(pbool('timestampQueryEnable'))
  )
  .fn(async t => {
    const { type, pipelineStatisticsQueryEnable, timestampQueryEnable } = t.params;

    const extensions: GPUExtensionName[] = [];
    if (pipelineStatisticsQueryEnable) {
      extensions.push('pipeline-statistics-query');
    }
    if (timestampQueryEnable) {
      extensions.push('timestamp-query');
    }

    await t.selectDeviceOrSkipTestCase({ extensions });

    const count = 1;
    const pipelineStatistics =
      type === 'pipeline-statistics' ? (['clipper-invocations'] as const) : ([] as const);
    const shouldError =
      (type === 'pipeline-statistics' && !pipelineStatisticsQueryEnable) ||
      (type === 'timestamp' && !timestampQueryEnable);

    t.expectValidationError(() => {
      t.device.createQuerySet({ type, count, pipelineStatistics });
    }, shouldError);
  });
