export const description = `
Tests for queries/filtering, loading, and running.
`;

import { TestFileLoader, SpecFile } from '../common/framework/file_loader.js';
import { Logger } from '../common/framework/logging/logger.js';
import { Status } from '../common/framework/logging/result.js';
import { parseQuery } from '../common/framework/query/parseQuery.js';
import {
  TestQuery,
  TestQuerySingleCase,
  TestQueryMultiCase,
  TestQueryMultiTest,
  TestQueryMultiFile,
} from '../common/framework/query/query.js';
import { makeTestGroup, makeTestGroupForUnitTesting } from '../common/framework/test_group.js';
import { TestSuiteListing, TestSuiteListingEntry } from '../common/framework/test_suite_listing.js';
import { TestTreeLeaf } from '../common/framework/tree.js';
import { assert, objectEquals } from '../common/framework/util/util.js';

import { UnitTest } from './unit_test.js';

const listingData: { [k: string]: TestSuiteListingEntry[] } = {
  suite1: [
    { file: [], readme: 'desc 1a' },
    { file: ['foo'], description: 'desc 1b' },
    { file: ['bar'], readme: 'desc 1c' },
    { file: ['bar', 'buzz', 'buzz'], description: 'desc 1d' },
    { file: ['baz'], description: 'desc 1e' },
  ],
  suite2: [
    { file: [], readme: 'desc 2a' },
    { file: ['foof'], description: 'desc 2b' },
  ],
};

const specsData: { [k: string]: SpecFile } = {
  'suite1/foo.spec.js': {
    description: 'desc 1b',
    g: (() => {
      const g = makeTestGroupForUnitTesting(UnitTest);
      g.test('hello').fn(() => {});
      g.test('bonjour').fn(() => {});
      g.test('hola').fn(() => {});
      return g;
    })(),
  },
  'suite1/bar/biz.spec.js': {
    description: 'desc 1f',
    g: makeTestGroupForUnitTesting(UnitTest),
  },
  'suite1/bar/bez.spec.js': {
    description: 'desc 1g',
    g: makeTestGroupForUnitTesting(UnitTest),
  },
  'suite1/bar/buzz/buzz.spec.js': {
    description: 'desc 1d',
    g: (() => {
      const g = makeTestGroupForUnitTesting(UnitTest);
      g.test('zap').fn(() => {});
      return g;
    })(),
  },
  'suite1/baz.spec.js': {
    description: 'desc 1e',
    g: (() => {
      const g = makeTestGroupForUnitTesting(UnitTest);
      g.test('wye')
        .params([{}, { x: 1 }])
        .fn(() => {});
      g.test('zed')
        .params([
          { a: 1, b: 2, _c: 0 },
          { b: 3, a: 1, _c: 0 },
        ])
        .fn(() => {});
      return g;
    })(),
  },
  'suite2/foof.spec.js': {
    description: 'desc 2b',
    g: (() => {
      const g = makeTestGroupForUnitTesting(UnitTest);
      g.test('blah').fn(t => {
        t.debug('OK');
      });
      g.test('bleh')
        .params([{ a: 1 }])
        .fn(t => {
          t.debug('OK');
          t.debug('OK');
        });
      g.test('bluh,a').fn(t => {
        t.fail('goodbye');
      });
      return g;
    })(),
  },
};

class FakeTestFileLoader extends TestFileLoader {
  async listing(suite: string): Promise<TestSuiteListing> {
    return listingData[suite];
  }

  async import(path: string): Promise<SpecFile> {
    assert(path in specsData, '[test] mock file ' + path + ' does not exist');
    return specsData[path];
  }
}

class LoadingTest extends UnitTest {
  static readonly loader = new FakeTestFileLoader();

  async load(query: string): Promise<TestTreeLeaf[]> {
    return Array.from(await LoadingTest.loader.loadCases(parseQuery(query)));
  }

  async loadNames(query: string): Promise<string[]> {
    return (await this.load(query)).map(c => c.query.toString());
  }
}

export const g = makeTestGroup(LoadingTest);

g.test('suite').fn(async t => {
  t.shouldReject('Error', t.load('suite1'));
  t.shouldReject('Error', t.load('suite1:'));
});

g.test('group').fn(async t => {
  t.expect((await t.load('suite1:*')).length === 8);
  t.expect((await t.load('suite1:foo,*')).length === 3); // x:foo,* matches x:foo:
  t.expect((await t.load('suite1:bar,*')).length === 1);
  t.expect((await t.load('suite1:bar,buzz,buzz,*')).length === 1);

  t.shouldReject('Error', t.load('suite1:f*'));

  {
    const s = new TestQueryMultiFile('suite1', ['bar', 'buzz']).toString();
    t.expect((await t.load(s)).length === 1);
  }
});

g.test('test').fn(async t => {
  t.shouldReject('Error', t.load('suite1::'));
  t.shouldReject('Error', t.load('suite1:bar:'));
  t.shouldReject('Error', t.load('suite1:bar,:'));

  t.shouldReject('Error', t.load('suite1::*'));
  t.shouldReject('Error', t.load('suite1:bar,:*'));
  t.shouldReject('Error', t.load('suite1:bar:*'));

  t.expect((await t.load('suite1:foo:*')).length === 3);
  t.expect((await t.load('suite1:bar,buzz,buzz:*')).length === 1);
  t.expect((await t.load('suite1:baz:*')).length === 4);

  t.expect((await t.load('suite2:foof:bluh,*')).length === 1);
  t.expect((await t.load('suite2:foof:bluh,a,*')).length === 1);

  {
    const s = new TestQueryMultiTest('suite2', ['foof'], ['bluh']).toString();
    t.expect((await t.load(s)).length === 1);
  }
});

g.test('case').fn(async t => {
  t.shouldReject('Error', t.load('suite1:foo::'));
  t.shouldReject('Error', t.load('suite1:bar:zed,:'));

  t.shouldReject('Error', t.load('suite1:foo:h*'));

  t.shouldReject('Error', t.load('suite1:foo::*'));
  t.shouldReject('Error', t.load('suite1:baz::*'));
  t.shouldReject('Error', t.load('suite1:baz:zed,:*'));

  t.shouldReject('Error', t.load('suite1:baz:zed:'));
  t.shouldReject('Error', t.load('suite1:baz:zed:a=1;b=2*'));
  t.shouldReject('Error', t.load('suite1:baz:zed:a=1;b=2;'));
  t.shouldReject('SyntaxError', t.load('suite1:baz:zed:a=1;b=2,')); // tries to parse '2,' as JSON
  t.shouldReject('Error', t.load('suite1:baz:zed:a=1,b=2')); // '=' not allowed in value '1,b=2'
  t.shouldReject('Error', t.load('suite1:baz:zed:b=2*'));
  t.shouldReject('Error', t.load('suite1:baz:zed:b=2;a=1;_c=0'));
  t.shouldReject('Error', t.load('suite1:baz:zed:a=1,*'));

  t.expect((await t.load('suite1:baz:zed:*')).length === 2);
  t.expect((await t.load('suite1:baz:zed:a=1;*')).length === 2);
  t.expect((await t.load('suite1:baz:zed:a=1;b=2')).length === 1);
  t.expect((await t.load('suite1:baz:zed:a=1;b=2;*')).length === 1);
  t.expect((await t.load('suite1:baz:zed:b=2;*')).length === 1);
  t.expect((await t.load('suite1:baz:zed:b=2;a=1')).length === 1);
  t.expect((await t.load('suite1:baz:zed:b=2;a=1;*')).length === 1);
  t.expect((await t.load('suite1:baz:zed:b=3;a=1')).length === 1);
  t.expect((await t.load('suite1:baz:zed:a=1;b=3')).length === 1);
  t.expect((await t.load('suite1:foo:hello:')).length === 1);

  {
    const s = new TestQueryMultiCase('suite1', ['baz'], ['zed'], { a: 1, b: 2 }).toString();
    t.expect((await t.load(s)).length === 1);
  }
  {
    const s = new TestQuerySingleCase('suite1', ['baz'], ['zed'], { a: 1, b: 2 }).toString();
    t.expect((await t.load(s)).length === 1);
  }
});

g.test('end2end').fn(async t => {
  const l = await t.load('suite2:foof:*');
  assert(l.length === 3, 'listing length');

  const log = new Logger(true);

  const exp = async (
    i: number,
    query: TestQuery,
    status: Status,
    logs: (s: string[]) => boolean
  ) => {
    t.expect(objectEquals(l[i].query, query));
    const name = l[i].query.toString();
    const [rec, res] = log.record(name);
    await l[i].run(rec);

    t.expect(log.results.get(name) === res);
    t.expect(res.status === status);
    t.expect(res.timems >= 0);
    assert(res.logs !== undefined); // only undefined while pending
    t.expect(logs(res.logs.map(l => JSON.stringify(l))));
  };

  await exp(0, new TestQuerySingleCase('suite2', ['foof'], ['blah'], {}), 'pass', logs =>
    objectEquals(logs, ['"DEBUG: OK"'])
  );
  await exp(1, new TestQuerySingleCase('suite2', ['foof'], ['bleh'], { a: 1 }), 'pass', logs =>
    objectEquals(logs, ['"DEBUG: OK"', '"DEBUG: OK"'])
  );
  await exp(
    2,
    new TestQuerySingleCase('suite2', ['foof'], ['bluh', 'a'], {}),
    'fail',
    logs =>
      logs.length === 1 &&
      logs[0].startsWith('"EXPECTATION FAILED: goodbye\\n') &&
      logs[0].indexOf('loaders_and_trees.spec.') !== -1
  );
});

async function testIterateCollapsed(
  t: LoadingTest,
  expectations: string[],
  expectedResult: 'throws' | string[]
) {
  const treePromise = LoadingTest.loader.loadTree(
    new TestQueryMultiFile('suite1', []),
    expectations
  );
  if (expectedResult === 'throws') {
    t.shouldReject('Error', treePromise, 'loadTree should have thrown Error');
    return;
  }
  const tree = await treePromise;
  const actual = Array.from(tree.iterateCollapsedQueries(), q => q.toString());
  if (!objectEquals(actual, expectedResult)) {
    t.fail(
      `iterateCollapsed failed:
  got [${actual.join(', ')}]
  exp [${expectedResult.join(', ')}]
${tree.toString()}`
    );
  }
}

g.test('print').fn(async () => {
  const tree = await LoadingTest.loader.loadTree(new TestQueryMultiFile('suite1', []));
  tree.toString();
});

g.test('iterateCollapsed').fn(async t => {
  // Expectations have no effect
  await testIterateCollapsed(t, [], ['suite1:foo:*', 'suite1:bar,buzz,buzz:*', 'suite1:baz:*']);
  await testIterateCollapsed(
    t,
    ['suite1:*'],
    ['suite1:foo:*', 'suite1:bar,buzz,buzz:*', 'suite1:baz:*']
  );
  await testIterateCollapsed(
    t,
    ['suite1:foo:*'],
    ['suite1:foo:*', 'suite1:bar,buzz,buzz:*', 'suite1:baz:*']
  );
  await testIterateCollapsed(
    t,
    ['suite1:bar,buzz,buzz:*'],
    ['suite1:foo:*', 'suite1:bar,buzz,buzz:*', 'suite1:baz:*']
  );

  // Expectations have some effect
  await testIterateCollapsed(
    t,
    ['suite1:baz:wye:*'],
    ['suite1:foo:*', 'suite1:bar,buzz,buzz:*', 'suite1:baz:wye:*', 'suite1:baz:zed,*']
  );
  await testIterateCollapsed(
    t,
    ['suite1:baz:zed:*'],
    ['suite1:foo:*', 'suite1:bar,buzz,buzz:*', 'suite1:baz:wye,*', 'suite1:baz:zed:*']
  );
  await testIterateCollapsed(
    t,
    ['suite1:baz:wye:*', 'suite1:baz:zed:*'],
    ['suite1:foo:*', 'suite1:bar,buzz,buzz:*', 'suite1:baz:wye:*', 'suite1:baz:zed:*']
  );
  await testIterateCollapsed(
    t,
    ['suite1:baz:wye:'],
    [
      'suite1:foo:*',
      'suite1:bar,buzz,buzz:*',
      'suite1:baz:wye:',
      'suite1:baz:wye:x=1;*',
      'suite1:baz:zed,*',
    ]
  );
  await testIterateCollapsed(
    t,
    ['suite1:baz:wye:x=1'],
    [
      'suite1:foo:*',
      'suite1:bar,buzz,buzz:*',
      'suite1:baz:wye:',
      'suite1:baz:wye:x=1',
      'suite1:baz:zed,*',
    ]
  );
  await testIterateCollapsed(
    t,
    ['suite1:foo:*', 'suite1:baz:wye:'],
    [
      'suite1:foo:*',
      'suite1:bar,buzz,buzz:*',
      'suite1:baz:wye:',
      'suite1:baz:wye:x=1;*',
      'suite1:baz:zed,*',
    ]
  );

  // Invalid expectation queries
  await testIterateCollapsed(t, ['garbage'], 'throws');
  await testIterateCollapsed(t, ['garbage*'], 'throws');
  await testIterateCollapsed(t, ['suite1*'], 'throws');
  await testIterateCollapsed(t, ['suite1:foo*'], 'throws');
  await testIterateCollapsed(t, ['suite1:foo:ba*'], 'throws');

  // Valid expectation queries but they don't match anything
  await testIterateCollapsed(t, ['garbage:*'], 'throws');
  await testIterateCollapsed(t, ['suite1:doesntexist:*'], 'throws');
  await testIterateCollapsed(t, ['suite2:foo:*'], 'throws');
  // Doesn't match anything because we collapse this unnecessary node into just 'suite1:foo:*'
  await testIterateCollapsed(t, ['suite1:foo,*'], 'throws');
});
