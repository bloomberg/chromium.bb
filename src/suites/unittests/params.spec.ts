export const description = `
Unit tests for parameterization.
`;

import { ParamSpec, TestGroup, pcombine, pexclude, pfilter } from '../../framework/index.js';

import { TestGroupTest } from './test_group_test.js';
import { UnitTest } from './unit_test.js';

export const g = new TestGroup(TestGroupTest);

g.test('none', t => {
  t.fail("this test shouldn't run");
}).params([]);

g.test('combine none', t => {
  t.fail("this test shouldn't run");
}).params(pcombine([]));

g.test('filter', t => {
  t.expect(t.params.a);
}).params(
  pfilter(
    [
      { a: true, x: 1 }, //
      { a: false, y: 2 },
    ],
    p => p.a
  )
);

g.test('exclude', t => {
  t.expect(t.params.a);
}).params(
  pexclude(
    [
      { a: true, x: 1 }, //
      { a: false, y: 2 },
    ],
    [
      { a: true }, //
      { a: false, y: 2 },
    ]
  )
);

g.test('generator', t0 => {
  const g = new TestGroup(UnitTest);

  const ran: ParamSpec[] = [];

  g.test('generator', t => {
    ran.push(t.params);
  }).params(
    (function*(): IterableIterator<ParamSpec> {
      for (let x = 0; x < 3; ++x) {
        for (let y = 0; y < 2; ++y) {
          yield { x, y };
        }
      }
    })()
  );

  t0.expectCases(g, [
    { test: 'generator', params: { x: 0, y: 0 } },
    { test: 'generator', params: { x: 0, y: 1 } },
    { test: 'generator', params: { x: 1, y: 0 } },
    { test: 'generator', params: { x: 1, y: 1 } },
    { test: 'generator', params: { x: 2, y: 0 } },
    { test: 'generator', params: { x: 2, y: 1 } },
  ]);
});
