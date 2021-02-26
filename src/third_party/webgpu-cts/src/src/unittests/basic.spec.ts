export const description = `
Basic unit tests for test framework.
`;

import { makeTestGroup } from '../common/framework/test_group.js';

import { UnitTest } from './unit_test.js';

export const g = makeTestGroup(UnitTest);

/* eslint-disable-next-line  @typescript-eslint/no-unused-vars */
g.test('test,sync').fn(t => {});

/* eslint-disable-next-line  @typescript-eslint/no-unused-vars */
g.test('test,async').fn(async t => {});

g.test('test_with_params,sync')
  .params([{}])
  .fn(t => {
    t.debug(JSON.stringify(t.params));
  });

g.test('test_with_params,async')
  .params([{}])
  .fn(async t => {
    t.debug(JSON.stringify(t.params));
  });

g.test('test_with_params,private_params')
  .params([
    { a: 1, b: 2, _result: 3 }, //
    { a: 4, b: -3, _result: 1 },
  ])
  .fn(t => {
    const { a, b, _result } = t.params;
    t.expect(a + b === _result);
  });
