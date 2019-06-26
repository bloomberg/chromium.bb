export const description = `
Unit tests for parameterization.
`;

import { TestGroup, DefaultFixture, pcombine, pfilter, pexclude } from '../../framework/index.js';

export const g = new TestGroup(DefaultFixture);

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
