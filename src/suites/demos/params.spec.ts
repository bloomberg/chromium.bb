export const description = `
Demos of parameterization system.
`;

import {
  DefaultFixture,
  TestGroup,
  poptions,
  pcombine,
  pfilter,
  pexclude,
} from '../../framework/index.js';

export const g = new TestGroup(DefaultFixture);

function print(t: DefaultFixture) {
  t.log(JSON.stringify(t.params));
}

g.test('literal', print).params([
  { hello: 1 }, //
  { hello: 2 },
]);

g.test('options', print).params(poptions('hello', [1, 2, 3]));

g.test('combine unit unit', print).params(
  pcombine([
    [{}], //
    [{}],
  ])
);

g.test('combine lists', print).params(
  pcombine([
    poptions('x', [1, 2]), //
    poptions('y', ['a', 'b']),
    [{}],
  ])
);

g.test('combine arrays', print).params(
  pcombine([
    [{ x: 1, y: 2 }, { x: 10, y: 20 }], //
    [{ z: 'z' }, { w: 'w' }],
  ])
);

g.test('combine mixed', print).params(
  pcombine([
    poptions('x', [1, 2]), //
    [{ z: 'z' }, { w: 'w' }],
  ])
);

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
