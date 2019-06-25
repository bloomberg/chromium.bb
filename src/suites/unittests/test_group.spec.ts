export const description = `
Unit tests for parameterization system.
`;

import { DefaultFixture, Fixture, TestGroup } from '../../framework/index.js';

export const group = new TestGroup(DefaultFixture);

group.test('default fixture', null, t0 => {
  function print(t: Fixture) {
    t.log(JSON.stringify(t.params));
  }

  const g = new TestGroup(DefaultFixture);

  g.test('test', null, print);
  g.test('testp', { a: 1 }, print);

  // TODO: check output
});

group.test('custom fixture', null, t0 => {
  class Printer extends DefaultFixture {
    print() {
      this.log(JSON.stringify(this.params));
    }
  }

  const g = new TestGroup(Printer);

  g.test('test', null, t => {
    t.print();
  });
  g.test('testp', { a: 1 }, t => {
    t.print();
  });

  // TODO: check output
});

group.test('duplicate test name', null, t => {
  const g = new TestGroup(DefaultFixture);
  g.test('abc', null, () => {});

  t.shouldThrow(() => {
    g.test('abc', null, () => {});
  });
});

for (const char of '"`~@#$+=\\|!^&*[]<>{}-\'.,') {
  group.test('invalid test name', { char }, t => {
    const g = new TestGroup(DefaultFixture);

    t.shouldThrow(() => {
      g.test('a' + char + 'b', null, () => {});
    });
  });
}
