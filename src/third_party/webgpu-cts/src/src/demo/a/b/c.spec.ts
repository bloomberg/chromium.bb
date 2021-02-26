export const description = 'Description for c.spec.ts';

import { makeTestGroup } from '../../../common/framework/test_group.js';
import { unreachable } from '../../../common/framework/util/util.js';
import { UnitTest } from '../../../unittests/unit_test.js';

export const g = makeTestGroup(UnitTest);

g.test('f')
  .desc(
    `Test plan for f
    - Test stuff
    - Test some more stuff`
  )
  .fn(() => {});

g.test('f,g').fn(() => {});

g.test('f,g,h')
  .params([{}, { x: 0 }, { x: 0, y: 0 }])
  .fn(() => {});

g.test('case_depth_2_in_single_child_test')
  .params([{ x: 0, y: 0 }])
  .fn(() => {});

g.test('statuses,debug').fn(t => {
  t.debug('debug');
});

g.test('statuses,skip').fn(t => {
  t.skip('skip');
});

g.test('statuses,warn').fn(t => {
  t.warn('warn');
});

g.test('statuses,fail').fn(t => {
  t.fail('fail');
});

g.test('statuses,throw').fn(() => {
  unreachable('unreachable');
});
