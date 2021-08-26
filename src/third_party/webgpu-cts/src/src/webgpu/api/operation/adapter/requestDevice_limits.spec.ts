export const description = `
Tests passing various requiredLimits to GPUAdapter.requestDevice.
`;

import { Fixture } from '../../../../common/framework/fixture.js';
import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { keysOf } from '../../../../common/util/data_tables.js';
import { assert } from '../../../../common/util/util.js';
import { DefaultLimits } from '../../../constants.js';
import { getGPU } from '../../../util/navigator_gpu.js';

const kLimitTypes = keysOf(DefaultLimits);

const kMaxUnsignedLongValue = 4294967295;
/** Clamps a numeric value to the valid unsigned long range, as defined by WebIDL */
function clampToUnsignedLong(value: number): number {
  return Math.min(kMaxUnsignedLongValue, Math.max(0, value));
}

export const g = makeTestGroup(Fixture);

g.test('unknown_limits')
  .desc(
    `
    Test that specifiying limits that aren't part of the supported limit set causes
    requestDevice to reject.`
  )
  .fn(async t => {
    const adapter = await navigator.gpu.requestAdapter();
    assert(adapter !== null);

    const requiredLimits: Record<string, number> = { unknownLimitName: 9000 };

    t.shouldReject('OperationError', adapter.requestDevice({ requiredLimits }));
  });

g.test('supported_limits')
  .desc(
    `
    Test that each supported limit can be specified with valid values.
    - Tests each limit with the default values given by the spec
    - Tests each limit with the supported values given by the adapter`
  )
  .params(u =>
    u
      .combine('limit', kLimitTypes)
      .unless(p => typeof DefaultLimits[p.limit] !== 'number')
      .beginSubcases()
      .combine('limitValue', ['default', 'adapter'])
  )
  .fn(async t => {
    const { limit, limitValue } = t.params;

    const gpu = getGPU();
    const adapter = await gpu.requestAdapter();
    assert(adapter !== null);

    let value: number = -1;
    switch (limitValue) {
      case 'default':
        value = DefaultLimits[limit] as number;
        break;
      case 'adapter':
        value = adapter.limits[limit] as number;
        break;
    }

    const device = await adapter.requestDevice({ requiredLimits: { [limit]: value } });
    assert(device !== null);
    t.expect(
      device.limits[limit] === value,
      'Devices reported limit should match the required limit'
    );
  });

g.test('better_than_supported')
  .desc(
    `
    Test that specifying a better limit than what the adapter supports causes requestDevice to
    reject.
    - Tests each limit
    - Tests requesting better limits by various amounts`
  )
  .params(u =>
    u
      .combine('limit', kLimitTypes)
      .unless(p => typeof DefaultLimits[p.limit] !== 'number')
      .beginSubcases()
      .combine('over', [1, 32, 65535])
  )
  .fn(async t => {
    const { limit, over } = t.params;

    const gpu = getGPU();
    const adapter = await gpu.requestAdapter();
    assert(adapter !== null);

    const mult = limit.startsWith('min') ? -1 : 1;

    const requiredLimits = {
      [limit]: clampToUnsignedLong(adapter.limits[limit] + over * mult),
    };

    t.shouldReject('OperationError', adapter.requestDevice({ requiredLimits }));
  });

g.test('worse_than_default')
  .desc(
    `
    Test that specifying a worse limit than the default values required by the spec cause the value
    to clamp.
    - Tests each limit
    - Tests requesting worse limits by various amounts`
  )
  .params(u =>
    u
      .combine('limit', kLimitTypes)
      .unless(p => typeof DefaultLimits[p.limit] !== 'number')
      .beginSubcases()
      .combine('under', [1, 32, 65535])
  )
  .fn(async t => {
    const { limit, under } = t.params;

    const gpu = getGPU();
    const adapter = await gpu.requestAdapter();
    assert(adapter !== null);

    const mult = limit.startsWith('min') ? -1 : 1;

    const requiredLimits = {
      [limit]: clampToUnsignedLong((DefaultLimits[limit] as number) - under * mult),
    };

    const device = await adapter.requestDevice({ requiredLimits });
    assert(device !== null);
    t.expect(
      device.limits[limit] === DefaultLimits[limit],
      'Devices reported limit should match the default limit'
    );
  });
