export const description = `
Unit tests for TestGroup.
`;

import { Fixture, TestGroup, assert, poptions } from '../../framework/index.js';

import { TestGroupTest } from './test_group_test.js';
import { UnitTest } from './unit_test.js';

export const g = new TestGroup(TestGroupTest);

g.test('UnitTest fixture', async t0 => {
  let seen = 0;
  function count(t: Fixture): void {
    seen++;
  }

  const g = new TestGroup(UnitTest);

  g.test('test', count);
  g.test('testp', count).params([{ a: 1 }]);

  await t0.run(g);
  t0.expect(seen === 2);
});

g.test('custom fixture', async t0 => {
  let seen = 0;
  class Counter extends UnitTest {
    count(): void {
      seen++;
    }
  }

  const g = new TestGroup(Counter);

  g.test('test', t => {
    t.count();
  });
  g.test('testp', t => {
    t.count();
  }).params([{ a: 1 }]);

  await t0.run(g);
  t0.expect(seen === 2);
});

g.test('stack', async t0 => {
  const g = new TestGroup(UnitTest);

  const doNestedThrow1 = () => {
    throw new Error('goodbye');
  };

  const doNestedThrow2 = () => doNestedThrow1();

  g.test('fail', t => {
    t.fail();
  });
  g.test('throw', t => {
    throw new Error('hello');
  });
  g.test('throw nested', t => {
    doNestedThrow2();
  });

  const res = await t0.run(g);

  const search = /unittests[\/\\]test_group\.spec\.[tj]s|suites[\/\\]unittests[\/\\]unit_test\.[tj]s/;
  for (const { logs } of res.cases) {
    assert(logs !== undefined, 'expected logs');
    const l = logs[0].toJSON();
    t0.expect(search.test(l));
    const st = l.split('\n');
    t0.expect(search.test(st[st.length - 1]));
  }
});

g.test('duplicate test name', t => {
  const g = new TestGroup(UnitTest);
  g.test('abc', () => {});

  t.shouldThrow('Error', () => {
    g.test('abc', () => {});
  });
});

g.test('duplicate test params', t => {
  const g = new TestGroup(UnitTest);

  t.shouldThrow('Error', () => {
    g.test('abc', () => {
      //
    }).params([
      { a: 1 }, //
      { a: 1 },
    ]);
  });
});

g.test('duplicate test params/with different private params', t => {
  const g = new TestGroup(UnitTest);

  t.shouldThrow('Error', () => {
    g.test('abc', () => {
      //
    }).params([
      { a: 1, _b: 1 }, //
      { a: 1, _b: 2 },
    ]);
  });
});

const badChars = Array.from('"`~@#$+=\\|!^&*[]<>{}-\'.,');
g.test('invalid test name', t => {
  const g = new TestGroup(UnitTest);

  t.shouldThrow('Error', () => {
    g.test('a' + t.params.char + 'b', () => {});
  });
}).params(poptions('char', badChars));

g.test('throws', async t0 => {
  const g = new TestGroup(UnitTest);

  g.test('a', t => {
    throw new Error();
  });

  const result = await t0.run(g);
  t0.expect(result.cases[0].status === 'fail');
});

g.test('shouldThrow', async t0 => {
  t0.shouldThrow('TypeError', () => {
    throw new TypeError();
  });

  const g = new TestGroup(UnitTest);

  g.test('a', t => {
    t.shouldThrow('Error', () => {
      throw new TypeError();
    });
  });

  const result = await t0.run(g);
  t0.expect(result.cases[0].status === 'fail');
});

g.test('shouldReject', async t0 => {
  t0.shouldReject(
    'TypeError',
    (async () => {
      throw new TypeError();
    })()
  );

  const g = new TestGroup(UnitTest);

  g.test('a', async t => {
    t.shouldReject(
      'Error',
      (async () => {
        throw new TypeError();
      })()
    );
  });

  const result = await t0.run(g);
  // Fails even though shouldReject doesn't fail until after the test function ends
  t0.expect(result.cases[0].status === 'fail');
});
