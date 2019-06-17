
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
  g.testp('testp', {a: 1}, DefaultFixture, print);

  // TODO: check output
});

group.test('custom fixture', DefaultFixture, (t0) => {
  class Printer extends DefaultFixture {
    public print() {
      this.log(JSON.stringify(this.params));
    }
  }

  const g = new TestGroup();

  g.test('test', Printer, (t) => { t.print(); });
  g.testp('testp', {a: 1}, Printer, (t) => { t.print(); });

  // TODO: check output
});

group.test('duplicate test name', DefaultFixture, (t) => {
  const g = new TestGroup();
  g.test('abc', DefaultFixture, () => {});

  t.shouldThrow(() => {
    g.test('abc', DefaultFixture, () => {});
  });
});

for (const char of '"`~@#$+=\\|!^&*[]<>{}-\'.,') {
  group.testp('invalid test name', {char}, DefaultFixture, (t) => {
    const g = new TestGroup();

    t.shouldThrow(() => {
      g.test('a' + char + 'b', DefaultFixture, () => {});
    });
  });
}
