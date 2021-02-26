export const description = `
Unit tests for parameterization helpers.
`;

import { poptions, params } from '../common/framework/params_builder.js';
import {
  CaseParams,
  CaseParamsIterable,
  publicParamsEquals,
} from '../common/framework/params_utils.js';
import { makeTestGroup } from '../common/framework/test_group.js';

import { UnitTest } from './unit_test.js';

class ParamsTest extends UnitTest {
  expectSpecEqual(act: CaseParamsIterable, exp: CaseParams[]): void {
    const a = Array.from(act);
    this.expect(a.length === exp.length && a.every((x, i) => publicParamsEquals(x, exp[i])));
  }
}

export const g = makeTestGroup(ParamsTest);

g.test('options').fn(t => {
  t.expectSpecEqual(poptions('hello', [1, 2, 3]), [{ hello: 1 }, { hello: 2 }, { hello: 3 }]);
});

g.test('params').fn(t => {
  t.expectSpecEqual(params(), [{}]);
});

g.test('combine,zeroes_and_ones').fn(t => {
  t.expectSpecEqual(params().combine([]).combine([]), []);
  t.expectSpecEqual(params().combine([]).combine([{}]), []);
  t.expectSpecEqual(params().combine([{}]).combine([]), []);
  t.expectSpecEqual(params().combine([{}]).combine([{}]), [{}]);
});

g.test('combine,mixed').fn(t => {
  t.expectSpecEqual(
    params()
      .combine(poptions('x', [1, 2]))
      .combine(poptions('y', ['a', 'b']))
      .combine([{ p: 4 }, { q: 5 }])
      .combine([{}]),
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

g.test('filter').fn(t => {
  t.expectSpecEqual(
    params()
      .combine([
        { a: true, x: 1 },
        { a: false, y: 2 },
      ])
      .filter(p => p.a),
    [{ a: true, x: 1 }]
  );
});

g.test('unless').fn(t => {
  t.expectSpecEqual(
    params()
      .combine([
        { a: true, x: 1 },
        { a: false, y: 2 },
      ])
      .unless(p => p.a),
    [{ a: false, y: 2 }]
  );
});

g.test('exclude').fn(t => {
  t.expectSpecEqual(
    params()
      .combine([
        { a: true, x: 1 },
        { a: false, y: 2 },
      ])
      .exclude([{ a: true }, { a: false, y: 2 }]),
    [{ a: true, x: 1 }]
  );
});

g.test('expand').fn(t => {
  t.expectSpecEqual(
    params()
      .combine([
        { a: true, x: 1 },
        { a: false, y: 2 },
      ])
      .expand(function* (p) {
        if (p.a) {
          yield* poptions('z', [3, 4]);
        } else {
          yield { w: 5 };
        }
      }),
    [
      { a: true, x: 1, z: 3 },
      { a: true, x: 1, z: 4 },
      { a: false, y: 2, w: 5 },
    ]
  );
});

g.test('expand,invalid').fn(t => {
  const p = params()
    .combine([{ x: 1 }])
    .expand(function* (p) {
      yield p; // causes key 'x' to be duplicated
    });
  t.shouldThrow('Error', () => {
    Array.from(p);
  });
});

g.test('undefined').fn(t => {
  t.expect(!publicParamsEquals({ a: undefined }, {}));
  t.expect(!publicParamsEquals({}, { a: undefined }));
});

g.test('private').fn(t => {
  t.expect(publicParamsEquals({ _a: 0 }, {}));
  t.expect(publicParamsEquals({}, { _a: 0 }));
});

g.test('arrays').fn(t => {
  t.expectSpecEqual([{ a: [1, 2] }], [{ a: [1, 2] }]);
});
