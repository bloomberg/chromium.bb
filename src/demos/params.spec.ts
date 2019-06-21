export const description = `
Demos of parameterization system.
`;

import {
  DefaultFixture,
  pcombine,
  pexclude,
  pfilter,
  poptions,
  TestGroup,
} from '../framework/index.js';

export const group = new TestGroup();

function print(t: DefaultFixture) {
  t.log(JSON.stringify(t.params));
}

for (const params of [{ hello: 1 }, { hello: 2 }]) {
  group.test('literal', params, DefaultFixture, print);
}

for (const params of poptions('hello', [1, 2, 3])) {
  group.test('options', params, DefaultFixture, print);
}

for (const params of pcombine([])) {
  group.test('combine none', params, DefaultFixture, t => {
    t.fail("this test shouldn't run");
  });
}

for (const params of pcombine([[{}], [{}]])) {
  group.test('combine unit unit', params, DefaultFixture, print);
}

for (const params of pcombine([poptions('x', [1, 2]), poptions('y', ['a', 'b']), [{}]])) {
  group.test('combine lists', params, DefaultFixture, print);
}

for (const params of pcombine([[{ x: 1, y: 2 }, { x: 10, y: 20 }], [{ z: 'z' }, { w: 'w' }]])) {
  group.test('combine arrays', params, DefaultFixture, print);
}

for (const params of pcombine([poptions('x', [1, 2]), [{ z: 'z' }, { w: 'w' }]])) {
  group.test('combine mixed', params, DefaultFixture, print);
}

for (const params of pfilter([{ a: true, x: 1 }, { a: false, y: 2 }], p => p.a)) {
  group.test('filter', params, DefaultFixture, t => {
    t.expect(t.params.a);
  });
}

for (const params of pexclude(
  [{ a: true, x: 1 }, { a: false, y: 2 }],
  [{ a: true }, { a: false, y: 2 }]
)) {
  group.test('exclude', params, DefaultFixture, t => {
    t.expect(t.params.a);
  });
}
