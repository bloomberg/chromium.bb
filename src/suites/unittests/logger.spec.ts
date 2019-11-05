export const description = `
Unit tests for namespaced logging system.

Also serves as a larger test of async test functions, and of the logging system.
`;

import { SkipTestCase, TestGroup, paramsEquals } from '../../framework/index.js';
import { Logger } from '../../framework/logger.js';

import { UnitTest } from './unit_test.js';

export const g = new TestGroup(UnitTest);

g.test('construct', t => {
  const mylog = new Logger();
  const [testrec, testres] = mylog.record({ suite: 'a', path: 'foo/bar' });
  const [, res1] = testrec.record('baz', null);
  const params2 = {};
  const [, res2] = testrec.record('qux', params2);

  t.expect(testres.spec === 'a:foo/bar:');
  t.expect(testres.cases.length === 2);
  t.expect(testres.cases[0] === res1);
  t.expect(testres.cases[1] === res2);
  t.expect(res1.test === 'baz');
  t.expect(res1.params === null);
  t.expect(res1.logs === undefined);
  t.expect(res1.status === 'running');
  t.expect(res1.timems < 0);
  t.expect(res2.test === 'qux');
  t.expect(paramsEquals(res2.params, params2));
  t.expect(res2.logs === undefined);
  t.expect(res2.status === 'running');
  t.expect(res2.timems < 0);
});

g.test('private params', t => {
  const mylog = new Logger();
  const [testrec] = mylog.record({ suite: 'a', path: 'foo/bar' });
  const [, res] = testrec.record('baz', { a: 1, _b: 2 });

  t.expect(paramsEquals(res.params, { a: 1 }));
});

g.test('empty', t => {
  const mylog = new Logger();
  const [testrec] = mylog.record({ suite: '', path: '' });
  const [rec, res] = testrec.record('baz', null);

  rec.start();
  t.expect(res.status === 'running');
  rec.finish();
  t.expect(res.status === 'pass');
  t.expect(res.timems >= 0);
});

g.test('pass', t => {
  const mylog = new Logger();
  const [testrec] = mylog.record({ suite: '', path: '' });
  const [rec, res] = testrec.record('baz', null);

  rec.start();
  rec.debug(new Error('hello'));
  t.expect(res.status === 'running');
  rec.finish();
  t.expect(res.status === 'pass');
  t.expect(res.timems >= 0);
});

g.test('skip', t => {
  const mylog = new Logger();
  const [testrec] = mylog.record({ suite: '', path: '' });
  const [rec, res] = testrec.record('baz', null);

  rec.start();
  t.expect(res.status === 'running');

  rec.skipped(new SkipTestCase());
  rec.debug(new Error('hello'));

  t.expect(res.status === 'running');
  rec.finish();

  t.expect(res.status === 'skip');
  t.expect(res.timems >= 0);
});

g.test('warn', t => {
  const mylog = new Logger();
  const [testrec] = mylog.record({ suite: '', path: '' });
  const [rec, res] = testrec.record('baz', null);

  rec.start();
  t.expect(res.status === 'running');

  rec.warn(new Error());
  rec.skipped(new SkipTestCase());

  t.expect(res.status === 'running');
  rec.finish();

  t.expect(res.status === 'warn');
  t.expect(res.timems >= 0);
});

g.test('fail', t => {
  const mylog = new Logger();
  const [testrec] = mylog.record({ suite: '', path: '' });
  const [rec, res] = testrec.record('baz', null);

  rec.start();
  t.expect(res.status === 'running');

  rec.fail(new Error('bye'));
  rec.warn(new Error());
  rec.skipped(new SkipTestCase());

  t.expect(res.status === 'running');
  rec.finish();

  t.expect(res.status === 'fail');
  t.expect(res.timems >= 0);
});

g.test('debug', t => {
  const { debug, _logsCount } = t.params;

  const mylog = new Logger();
  const [testrec] = mylog.record({ suite: '', path: '' });
  const [rec, res] = testrec.record('baz', null);

  rec.start(debug);
  rec.debug(new Error('hello'));
  rec.finish();
  t.expect(res.status === 'pass');
  t.expect(res.timems >= 0);
  t.expect(res.logs!.length === _logsCount);
}).params([
  { debug: true, _logsCount: 1 }, //
  { debug: false, _logsCount: 0 },
]);
