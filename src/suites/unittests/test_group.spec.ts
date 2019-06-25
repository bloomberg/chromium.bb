export const description = `
Unit tests for parameterization system.
`;

import { DefaultFixture, Fixture, TestGroup, poptions } from '../../framework/index.js';

export const g = new TestGroup(DefaultFixture);

g.test('default fixture', t0 => {
  function print(t: Fixture) {
    t.log(JSON.stringify(t.params));
  }

  const g = new TestGroup(DefaultFixture);

  g.test('test', print);
  g.test('testp', print).params([{ a: 1 }]);

  // TODO: check output
});

g.test('custom fixture', t0 => {
  class Printer extends DefaultFixture {
    print() {
      this.log(JSON.stringify(this.params));
    }
  }

  const g = new TestGroup(Printer);

  g.test('test', t => {
    t.print();
  });
  g.test('testp', t => {
    t.print();
  }).params([{ a: 1 }]);

  // TODO: check output
});

g.test('duplicate test name', t => {
  const g = new TestGroup(DefaultFixture);
  g.test('abc', () => {});

  t.shouldThrow(() => {
    g.test('abc', () => {});
  });
});

const badChars = Array.from('"`~@#$+=\\|!^&*[]<>{}-\'.,');
g.test('invalid test name', t => {
  const g = new TestGroup(DefaultFixture);

  t.shouldThrow(() => {
    g.test('a' + t.params.char + 'b', () => {});
  });
}).params(poptions('char', badChars));
