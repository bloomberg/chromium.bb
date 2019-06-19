export const description = `
Unit tests for loader and test filter queries.
`;

import { DefaultFixture, objectEquals, paramsEquals, TestGroup } from '../framework/index.js';
import { IGroupDesc, IListing, ITestNode, TestLoader } from '../framework/loader.js';
import { Logger } from '../framework/logger.js';

const listingData: { [k: string]: IGroupDesc[] } = {
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

const nodesData: { [k: string]: ITestNode } = {
  'suite2/foof.spec.js': {
    description: 'desc 2b',
    group: (() => {
      const g = new TestGroup();
      g.test('blah', DefaultFixture, (t) => { t.ok(); });
      g.test('bleh', DefaultFixture, (t) => { t.ok(); t.ok(); }, {});
      return g;
    })(),
  },
};

export class TestTestLoader extends TestLoader {
  protected async fetch(_outDir: string, suite: string): Promise<IListing> {
    return { suite, groups: listingData[suite] };
  }

  protected async import(path: string): Promise<ITestNode> {
    return nodesData[path];
  }
}

class F extends DefaultFixture {
  public loader: TestTestLoader = new TestTestLoader();

  public async load(filters: string[]) {
    const listing = await this.loader.loadTests('./out', filters);
    const entries = Promise.all(Array.from(listing, ({ suite, path, node }) => node.then((n: ITestNode) => ({ suite, path, node: n }))));
    return entries;
  }
}

export const group = new TestGroup();

group.test('basic', F, async (t) => {
  const l = await t.load(['suite2:foof']);
  if (l.length !== 1) { throw new Error('listing length'); }

  t.expect(l[0].suite === 'suite2');
  t.expect(l[0].path === 'foof');
  t.expect(l[0].node.description === 'desc 2b');
  if (l[0].node.group === undefined) { throw new Error(); }
  t.expect(l[0].node.group.iterate instanceof Function);

  const log = new Logger();
  const [res, rec] = log.record(l[0].path);
  const rcs = Array.from(l[0].node.group.iterate(rec));
  if (rcs.length !== 2) { throw new Error('iterate length'); }

  t.expect(rcs[0].testcase.name === 'blah');
  t.expect(rcs[0].testcase.params === undefined);
  // TODO: testcase.run probably should be private
  t.expect(rcs[0].testcase.run instanceof Function);

  t.expect(rcs[1].testcase.name === 'bleh');
  t.expect(paramsEquals(rcs[1].testcase.params, {}));
  t.expect(rcs[1].testcase.run instanceof Function);

  t.expect(log.results[0] === res);
  t.expect(res.path === 'foof');
  t.expect(res.cases.length === 0);

  {
    const cases = res.cases;
    const res0 = await rcs[0].run();
    if (cases.length !== 1) { throw new Error('results cases length'); }
    t.expect(res.cases[0] === res0);
    t.expect(res0.name === 'blah');
    t.expect(res0.params === undefined);
    t.expect(res0.status === 'pass');
    t.expect(res0.timems > 0);
    if (res0.logs === undefined) { throw new Error('results case logs'); }
    t.expect(objectEquals(res0.logs, ['OK']));
  }
  {
    // Store cases off to a separate variable due to a typescript bug
    // where it can't detect that res.cases.length might change between
    // above and here.
    const cases = res.cases;
    const res1 = await rcs[1].run();
    if (cases.length !== 2) { throw new Error('results cases length'); }
    t.expect(res.cases[1] === res1);
    t.expect(res1.name === 'bleh');
    t.expect(paramsEquals(res1.params, {}));
    t.expect(res1.status === 'pass');
    t.expect(res1.timems > 0);
    if (res1.logs === undefined) { throw new Error('results case logs'); }
    t.expect(objectEquals(res1.logs, ['OK', 'OK']));
  }
});
