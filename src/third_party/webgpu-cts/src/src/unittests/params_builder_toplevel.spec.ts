export const description = `
Unit tests for parameterization.
`;

import { params } from '../common/framework/params_builder.js';
import { CaseParams } from '../common/framework/params_utils.js';
import { makeTestGroup, makeTestGroupForUnitTesting } from '../common/framework/test_group.js';

import { TestGroupTest } from './test_group_test.js';
import { UnitTest } from './unit_test.js';

export const g = makeTestGroup(TestGroupTest);

g.test('none')
  .params([])
  .fn(t => {
    t.fail("this test shouldn't run");
  });

g.test('combine_none')
  .params(params().combine([]))
  .fn(t => {
    t.fail("this test shouldn't run");
  });

g.test('filter')
  .params(
    params()
      .combine([
        { a: true, x: 1 }, //
        { a: false, y: 2 },
      ])
      .filter(p => p.a)
  )
  .fn(t => {
    t.expect(t.params.a);
  });

g.test('unless')
  .params(
    params()
      .combine([
        { a: true, x: 1 }, //
        { a: false, y: 2 },
      ])
      .unless(p => p.a)
  )
  .fn(t => {
    t.expect(!t.params.a);
  });

g.test('exclude')
  .params(
    params()
      .combine([
        { a: true, x: 1 },
        { a: false, y: 2 },
      ])
      .exclude([
        { a: true }, //
        { a: false, y: 2 },
      ])
  )
  .fn(t => {
    t.expect(t.params.a);
  });

g.test('generator').fn(t0 => {
  const g = makeTestGroupForUnitTesting(UnitTest);

  const ran: CaseParams[] = [];

  g.test('generator')
    .params(
      (function* (): IterableIterator<CaseParams> {
        for (let x = 0; x < 3; ++x) {
          for (let y = 0; y < 2; ++y) {
            yield { x, y };
          }
        }
      })()
    )
    .fn(t => {
      ran.push(t.params);
    });

  t0.expectCases(g, [
    { test: ['generator'], params: { x: 0, y: 0 } },
    { test: ['generator'], params: { x: 0, y: 1 } },
    { test: ['generator'], params: { x: 1, y: 0 } },
    { test: ['generator'], params: { x: 1, y: 1 } },
    { test: ['generator'], params: { x: 2, y: 0 } },
    { test: ['generator'], params: { x: 2, y: 1 } },
  ]);
});
