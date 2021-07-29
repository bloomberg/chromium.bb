export const description = `
Tests for validation in createBuffer.
`;

import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { assert } from '../../../../common/util/util.js';
import { kBufferSizeAlignment } from '../../../capability_info.js';
import { ValidationTest } from '../validation_test.js';

export const g = makeTestGroup(ValidationTest);

assert(kBufferSizeAlignment === 4);
g.test('size')
  .desc('Test buffer size alignment.')
  .params(u =>
    u
      .combine('mappedAtCreation', [false, true])
      .beginSubcases()
      .combine('size', [
        0,
        kBufferSizeAlignment * 0.5,
        kBufferSizeAlignment,
        kBufferSizeAlignment * 1.5,
        kBufferSizeAlignment * 2,
      ])
  )
  .unimplemented();

g.test('usage')
  .desc('Test combinations of (one to two?) usage flags.')
  .params(u =>
    u //
      .beginSubcases()
      .combine('mappedAtCreation', [false, true])
      .combine('usage', [
        // TODO
      ])
  )
  .unimplemented();
