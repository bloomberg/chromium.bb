export const description = `
Tests for GPUCanvasContext.getSwapChainPreferredFormat.
`;

import { Fixture } from '../../../common/framework/fixture.js';
import { makeTestGroup } from '../../../common/framework/test_group.js';

export const g = makeTestGroup(Fixture);

g.test('value')
  .desc(`Ensure getSwapChainPreferredFormat returns one of the valid values.`)
  .unimplemented();
