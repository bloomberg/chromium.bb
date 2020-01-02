export const description = `
Unit tests for parameterization helpers.
`;

import {
  ParamSpec,
  ParamSpecIterable,
  TestGroup,
  paramsEquals,
  pcombine,
  pexclude,
  pfilter,
  poptions,
} from '../../framework/index.js';

import { UnitTest } from './unit_test.js';

class ParamsTest extends UnitTest {
  expectSpecEqual(act: ParamSpecIterable, exp: ParamSpec[]): void {
    const a = Array.from(act);
    this.expect(a.length === exp.length && a.every((x, i) => paramsEquals(x, exp[i])));
  }
}

export const g = new TestGroup(ParamsTest);

g.test('options', t => {
  t.expectSpecEqual(poptions('hello', [1, 2, 3]), [{ hello: 1 }, { hello: 2 }, { hello: 3 }]);
});

g.test('combine/none', t => {
  t.expectSpecEqual(pcombine(), []);
});

g.test('combine/zeroes and ones', t => {
  t.expectSpecEqual(pcombine([], []), []);
  t.expectSpecEqual(pcombine([], [{}]), []);
  t.expectSpecEqual(pcombine([{}], []), []);
  t.expectSpecEqual(pcombine([{}], [{}]), [{}]);
});

g.test('combine/mixed', t => {
  t.expectSpecEqual(
    pcombine(poptions('x', [1, 2]), poptions('y', ['a', 'b']), [{ p: 4 }, { q: 5 }], [{}]),
    [
      { x: 1, y: 'a', p: 4 },
      { x: 1, y: 'a', q: 5 },
      { x: 1, y: 'b', p: 4 },
      { x: 1, y: 'b', q: 5 },
      { x: 2, y: 'a', p: 4 },
      { x: 2, y: 'a', q: 5 },
      { x: 2, y: 'b', p: 4 },
      { x: 2, y: 'b', q: 5 },
    ]
  );
});

g.test('filter', t => {
  t.expectSpecEqual(
    pfilter(
      [
        { a: true, x: 1 },
        { a: false, y: 2 },
      ],
      p => p.a
    ),
    [{ a: true, x: 1 }]
  );
});

g.test('exclude', t => {
  t.expectSpecEqual(
    pexclude(
      [
        { a: true, x: 1 },
        { a: false, y: 2 },
      ],
      [{ a: true }, { a: false, y: 2 }]
    ),
    [{ a: true, x: 1 }]
  );
});

g.test('undefined', t => {
  t.expectSpecEqual([{ a: undefined }], [{}]);
  t.expectSpecEqual([{}], [{ a: undefined }]);
});

g.test('arrays', t => {
  t.expectSpecEqual([{ a: [1, 2] }], [{ a: [1, 2] }]);
});
