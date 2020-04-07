export const description = `
Checks that glslang is available. If glslang is not supposed to be available, suppress this test.
`;

import { Fixture } from '../../common/framework/fixture.js';
import { TestGroup } from '../../common/framework/test_group.js';
import { unreachable } from '../../common/framework/util/util.js';
import { initGLSL } from '../../common/glslang.js';

export const g = new TestGroup(Fixture);

g.test('check', async t => {
  // try{} to prevent the SkipTestCase exception from propagating.
  try {
    await initGLSL();
  } catch (ex) {
    unreachable(String(ex));
  }
});
