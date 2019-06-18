
export const description = `
Unit tests for parameterization system.
`;

import {
  DefaultFixture,
  Fixture,
  TestGroup,
} from '../framework/index.js';

export const group = new TestGroup();

group.test('default fixture', DefaultFixture, (t0) => {
  function print(t: Fixture) {
    t.log(JSON.stringify(t.params));
  }

  const g = new TestGroup();

  g.test('test', DefaultFixture, print);
  g.test('testp', DefaultFixture, print, { a: 1 });

  // TODO: check output
});

group.test('custom fixture', DefaultFixture, (t0) => {
  class Printer extends DefaultFixture {
    public print() {
      this.log(JSON.stringify(this.params));
    }
  }

  const g = new TestGroup();

  g.test('test', Printer, (t) => {
    t.print();
  });
  g.test('testp', Printer, (t) => {
    t.print();
  }, { a: 1 });

  // TODO: check output
});

group.test('duplicate test name', DefaultFixture, (t) => {
  const g = new TestGroup();
  g.test('abc', DefaultFixture, () => { });

  t.shouldThrow(() => {
    g.test('abc', DefaultFixture, () => { });
  });
});

for (const char of '"`~@#$+=\\|!^&*[]<>{}-\'.,') {
  group.test('invalid test name', DefaultFixture, (t) => {
    const g = new TestGroup();

    t.shouldThrow(() => {
      g.test('a' + char + 'b', DefaultFixture, () => { });
    });
  }, { char });
}
