export const description = `
Tests for queries/filtering, loading, and running.
`;

import { generateMinimalQueryList } from '../../framework/generate_minimal_query_list.js';
import { RunCase, TestGroup, paramsEquals } from '../../framework/index.js';
import { TestSuiteListing, TestSuiteListingEntry } from '../../framework/listing.js';
import { TestFileLoader, TestLoader, TestSpecOrReadme } from '../../framework/loader.js';
import { Logger } from '../../framework/logger.js';
import { TestFilterResult } from '../../framework/test_filter/index.js';
import { makeQueryString } from '../../framework/url_query.js';
import { assert, objectEquals } from '../../framework/util/index.js';

import { UnitTest } from './unit_test.js';

const listingData: { [k: string]: TestSuiteListingEntry[] } = {
  suite1: [
    { path: '', description: 'desc 1a' },
    { path: 'foo', description: 'desc 1b' },
    { path: 'bar/', description: 'desc 1c' },
    { path: 'bar/buzz', description: 'desc 1d' },
    { path: 'baz', description: 'desc 1e' },
  ],
  suite2: [
    { path: '', description: 'desc 2a' },
    { path: 'foof', description: 'desc 2b' },
  ],
};

const specsData: { [k: string]: TestSpecOrReadme } = {
  'suite1/README.txt': { description: 'desc 1a' },
  'suite1/foo.spec.js': {
    description: 'desc 1b',
    g: (() => {
      const g = new TestGroup(UnitTest);
      g.test('hello', () => {});
      g.test('bonjour', () => {});
      g.test('hola', () => {});
      return g;
    })(),
  },
  'suite1/bar/README.txt': { description: 'desc 1c' },
  'suite1/bar/biz.spec.js': {
    description: 'desc 1f',
    g: new TestGroup(UnitTest),
  },
  'suite1/bar/bez.spec.js': {
    description: 'desc 1g',
    g: new TestGroup(UnitTest),
  },
  'suite1/bar/buzz.spec.js': {
    description: 'desc 1d',
    g: (() => {
      const g = new TestGroup(UnitTest);
      g.test('zap', () => {});
      return g;
    })(),
  },
  'suite1/baz.spec.js': {
    description: 'desc 1e',
    g: (() => {
      const g = new TestGroup(UnitTest);
      g.test('wye', () => {}).params([
        {}, //
        { x: 1 },
      ]);
      g.test('zed', () => {}).params([
        { a: 1, b: 2, _c: 0 }, //
        { a: 1, b: 3, _c: 0 },
      ]);
      return g;
    })(),
  },
  'suite2/foof.spec.js': {
    description: 'desc 2b',
    g: (() => {
      const g = new TestGroup(UnitTest);
      g.test('blah', t => {
        t.debug('OK');
      });
      g.test('bleh', t => {
        t.debug('OK');
        t.debug('OK');
      }).params([{}]);
      g.test('bluh', t => {
        t.fail('bye');
      });
      return g;
    })(),
  },
};

class FakeTestFileLoader implements TestFileLoader {
  async listing(suite: string): Promise<TestSuiteListing> {
    return listingData[suite];
  }

  async import(path: string): Promise<TestSpecOrReadme> {
    assert(specsData.hasOwnProperty(path), '[test] mock file ' + path + ' does not exist');
    return specsData[path];
  }
}

class LoadingTest extends UnitTest {
  static readonly loader: TestLoader = new TestLoader(new FakeTestFileLoader());

  async load(filters: string[]): Promise<TestFilterResult[]> {
    return Array.from(await LoadingTest.loader.loadTestsFromCmdLine(filters));
  }

  async singleGroup(query: string): Promise<RunCase[]> {
    const [rec] = new Logger().record({ suite: '', path: '' });
    const a = await this.load([query]);
    assert(a.length === 1, 'more than one group');
    const spec = a[0].spec;
    assert('g' in spec, 'group undefined');
    return Array.from(spec.g.iterate(rec));
  }
}

export const g = new TestGroup(LoadingTest);

g.test('whole suite', async t => {
  t.shouldReject('Error', t.load(['suite1']));
  t.expect((await t.load(['suite1:'])).length === 5);
});

g.test('partial suite', async t => {
  t.expect((await t.load(['suite1:f'])).length === 1);
  t.expect((await t.load(['suite1:fo'])).length === 1);
  t.expect((await t.load(['suite1:foo'])).length === 1);
  t.expect((await t.load(['suite1:foof'])).length === 0);
  t.expect((await t.load(['suite1:ba'])).length === 3);
  t.expect((await t.load(['suite1:bar'])).length === 2);
  t.expect((await t.load(['suite1:bar/'])).length === 2);
  t.expect((await t.load(['suite1:bar/b'])).length === 1);
});

g.test('whole group', async t => {
  t.shouldReject('Error', t.load(['suite1::']));
  t.shouldReject('Error', t.load(['suite1:bar:']));
  t.shouldReject('Error', t.load(['suite1:bar/:']));
  t.expect((await t.singleGroup('suite1:bar/buzz:')).length === 1);
  t.expect((await t.singleGroup('suite1:baz:')).length === 4);

  {
    const foo = (await t.load(['suite1:foo:']))[0];
    t.expect(foo.id.suite === 'suite1');
    t.expect(foo.id.path === 'foo');
    assert('g' in foo.spec, 'foo group');
    const [rec] = new Logger().record({ suite: '', path: '' });
    t.expect(Array.from(foo.spec.g.iterate(rec)).length === 3);
  }
});

g.test('partial group', async t => {
  t.expect((await t.singleGroup('suite1:foo:h')).length === 2);
  t.expect((await t.singleGroup('suite1:foo:he')).length === 1);
  t.expect((await t.singleGroup('suite1:foo:hello')).length === 1);
  t.expect((await t.singleGroup('suite1:baz:zed')).length === 2);
});

g.test('partial test/exact', async t => {
  t.expect((await t.singleGroup('suite1:foo:hello=')).length === 1);
  t.expect((await t.singleGroup('suite1:baz:zed=')).length === 0);
  t.expect((await t.singleGroup('suite1:baz:zed=')).length === 0);
  t.expect((await t.singleGroup('suite1:baz:zed={}')).length === 0);
  t.expect((await t.singleGroup('suite1:baz:zed={"a":1,"b":2}')).length === 1);
  t.expect((await t.singleGroup('suite1:baz:zed={"b":2,"a":1}')).length === 1);
  t.expect((await t.singleGroup('suite1:baz:zed={"a":1,"b":2,"_c":0}')).length === 0);
});

g.test('partial test/makeQueryString', async t => {
  const s = makeQueryString(
    { suite: 'suite1', path: 'baz' },
    { test: 'zed', params: { a: 1, b: 2 } }
  );
  t.expect((await t.singleGroup(s)).length === 1);
});

g.test('partial test/match', async t => {
  t.expect((await t.singleGroup('suite1:baz:zed~')).length === 2);
  t.expect((await t.singleGroup('suite1:baz:zed~{}')).length === 2);
  t.expect((await t.singleGroup('suite1:baz:zed~{"a":1}')).length === 2);
  t.expect((await t.singleGroup('suite1:baz:zed~{"a":1,"b":2}')).length === 1);
  t.expect((await t.singleGroup('suite1:baz:zed~{"b":2,"a":1}')).length === 1);
  t.expect((await t.singleGroup('suite1:baz:zed~{"b":2}')).length === 1);
  t.expect((await t.singleGroup('suite1:baz:zed~{"a":2}')).length === 0);
  t.expect((await t.singleGroup('suite1:baz:zed~{"c":0}')).length === 0);
  t.expect((await t.singleGroup('suite1:baz:zed~{"_c":0}')).length === 0);
});

g.test('end2end', async t => {
  const l = await t.load(['suite2:foof']);
  assert(l.length === 1, 'listing length');

  t.expect(l[0].id.suite === 'suite2');
  t.expect(l[0].id.path === 'foof');
  t.expect(l[0].spec.description === 'desc 2b');
  assert('g' in l[0].spec);
  t.expect(l[0].spec.g.iterate instanceof Function);

  const log = new Logger();
  const [rec, res] = log.record(l[0].id);
  const rcs = Array.from(l[0].spec.g.iterate(rec));
  assert(rcs.length === 3, 'iterate length');

  t.expect(rcs[0].id.test === 'blah');
  t.expect(rcs[0].id.params === null);

  t.expect(rcs[1].id.test === 'bleh');
  t.expect(paramsEquals(rcs[1].id.params, {}));

  t.expect(log.results[0] === res);
  t.expect(res.spec === 'suite2:foof:');
  t.expect(res.cases.length === 0);

  {
    const cases = res.cases;
    const res0 = await rcs[0].run(true);
    assert(cases.length === 1, 'results cases length');
    t.expect(res.cases[0] === res0);
    t.expect(res0.test === 'blah');
    t.expect(res0.params === null);
    t.expect(res0.status === 'pass');
    t.expect(res0.timems >= 0);
    assert(res0.logs !== undefined, 'results case logs');
    t.expect(objectEquals(JSON.stringify(res0.logs), '["DEBUG: OK"]'));
  }
  {
    // Store cases off to a separate variable due to a typescript bug
    // where it can't detect that res.cases.length might change between
    // above and here.
    const cases = res.cases;
    const res1 = await rcs[1].run(true);
    assert(cases.length === 2, 'results cases length');
    t.expect(res.cases[1] === res1);
    t.expect(res1.test === 'bleh');
    t.expect(paramsEquals(res1.params, {}));
    t.expect(res1.status === 'pass');
    t.expect(res1.timems >= 0);
    assert(res1.logs !== undefined, 'results case logs');
    t.expect(objectEquals(JSON.stringify(res1.logs), '["DEBUG: OK","DEBUG: OK"]'));
  }
  {
    // Store cases off to a separate variable due to a typescript bug
    // where it can't detect that res.cases.length might change between
    // above and here.
    const cases = res.cases;
    const res2 = await rcs[2].run(true);
    assert(cases.length === 3, 'results cases length');
    t.expect(res.cases[2] === res2);
    t.expect(res2.test === 'bluh');
    t.expect(res2.params === null);
    t.expect(res2.status === 'fail');
    t.expect(res2.timems >= 0);
    assert(res2.logs !== undefined, 'results case logs');
    t.expect(res2.logs.length === 1);
    const l = res2.logs[0].toJSON();
    t.expect(l.startsWith('FAIL: bye\n'));
    t.expect(l.indexOf('loading.spec.') !== -1);
  }
});

const testGenerateMinimalQueryList = async (
  t: LoadingTest,
  expectations: string[],
  result: string[]
) => {
  const l = await t.load(['suite1:']);
  const queries = await generateMinimalQueryList(l, expectations);
  t.expect(objectEquals(queries, result));
};

g.test('generateMinimalQueryList/errors', async t => {
  t.shouldReject('Error', testGenerateMinimalQueryList(t, ['garbage'], []));
  t.shouldReject('Error', testGenerateMinimalQueryList(t, ['suite1:'], []));
  t.shouldReject('Error', testGenerateMinimalQueryList(t, ['suite1:foo'], []));
  t.shouldReject('Error', testGenerateMinimalQueryList(t, ['suite1:foo:ba'], []));
  t.shouldReject('Error', testGenerateMinimalQueryList(t, ['suite2:foo:'], []));
});

g.test('generateMinimalQueryList', async t => {
  await testGenerateMinimalQueryList(
    t, //
    [],
    ['suite1:foo:', 'suite1:bar/buzz:', 'suite1:baz:']
  );
  await testGenerateMinimalQueryList(
    t, //
    ['suite1:foo:'],
    ['suite1:foo:', 'suite1:bar/buzz:', 'suite1:baz:']
  );
  await testGenerateMinimalQueryList(
    t, //
    ['suite1:bar/buzz:'],
    ['suite1:foo:', 'suite1:bar/buzz:', 'suite1:baz:']
  );
  await testGenerateMinimalQueryList(
    t, //
    ['suite1:baz:wye~'],
    ['suite1:foo:', 'suite1:bar/buzz:', 'suite1:baz:wye~', 'suite1:baz:zed~']
  );
  await testGenerateMinimalQueryList(
    t, //
    ['suite1:baz:zed~'],
    ['suite1:foo:', 'suite1:bar/buzz:', 'suite1:baz:wye~', 'suite1:baz:zed~']
  );
  await testGenerateMinimalQueryList(
    t, //
    ['suite1:baz:wye~', 'suite1:baz:zed~'],
    ['suite1:foo:', 'suite1:bar/buzz:', 'suite1:baz:wye~', 'suite1:baz:zed~']
  );
  await testGenerateMinimalQueryList(
    t, //
    ['suite1:baz:wye='],
    [
      'suite1:foo:',
      'suite1:bar/buzz:',
      'suite1:baz:wye=',
      'suite1:baz:wye={"x":1}',
      'suite1:baz:zed~',
    ]
  );
  await testGenerateMinimalQueryList(
    t, //
    ['suite1:baz:wye={"x":1}'],
    [
      'suite1:foo:',
      'suite1:bar/buzz:',
      'suite1:baz:wye=',
      'suite1:baz:wye={"x":1}',
      'suite1:baz:zed~',
    ]
  );
  await testGenerateMinimalQueryList(
    t, //
    ['suite1:foo:', 'suite1:baz:wye='],
    [
      'suite1:foo:',
      'suite1:bar/buzz:',
      'suite1:baz:wye=',
      'suite1:baz:wye={"x":1}',
      'suite1:baz:zed~',
    ]
  );
});
