export const description = `
Unit tests for parameterization helpers.
`;

import { DefaultFixture } from '../../framework/default_fixture.js';
import {
  ParamsSpec,
  paramsEquals,
  ParamSpecIterable,
  pcombine,
  pexclude,
  pfilter,
  poptions,
  TestGroup,
} from '../../framework/index.js';

class ParamsTest extends DefaultFixture {
  expectSpecEqual(act: ParamSpecIterable, exp: ParamsSpec[]) {
    const a = Array.from(act);
    this.expect(a.length === exp.length && a.every((x, i) => paramsEquals(x, exp[i])));
  }
}

export const g = new TestGroup(ParamsTest);

g.test('options', t => {
  t.expectSpecEqual(poptions('hello', [1, 2, 3]), [{ hello: 1 }, { hello: 2 }, { hello: 3 }]);
});

g.test('combine/none', t => {
  t.expectSpecEqual(pcombine([]), []);
});

g.test('combine/zeroes and ones', t => {
  t.expectSpecEqual(pcombine([[], []]), []);
  t.expectSpecEqual(pcombine([[], [{}]]), []);
  t.expectSpecEqual(pcombine([[{}], []]), []);
  t.expectSpecEqual(pcombine([[{}], [{}]]), [{}]);
});

g.test('combine/mixed', t => {
  t.expectSpecEqual(
    pcombine([poptions('x', [1, 2]), poptions('y', ['a', 'b']), [{ p: 4 }, { q: 5 }], [{}]]),
    [
      { p: 4, x: 1, y: 'a' },
      { q: 5, x: 1, y: 'a' },
      { p: 4, x: 1, y: 'b' },
      { q: 5, x: 1, y: 'b' },
      { p: 4, x: 2, y: 'a' },
      { q: 5, x: 2, y: 'a' },
      { p: 4, x: 2, y: 'b' },
      { q: 5, x: 2, y: 'b' },
    ]
  );
});

g.test('filter', t => {
  t.expectSpecEqual(pfilter([{ a: true, x: 1 }, { a: false, y: 2 }], p => p.a), [
    { a: true, x: 1 },
  ]);
});

g.test('exclude', t => {
  t.expectSpecEqual(
    pexclude([{ a: true, x: 1 }, { a: false, y: 2 }], [{ a: true }, { a: false, y: 2 }]),
    [{ a: true, x: 1 }]
  );
});
