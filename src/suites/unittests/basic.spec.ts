export const description = `
Basic unit tests for test framework.
`;

import { TestGroup } from '../../framework/index.js';

import { UnitTest } from './unit_test.js';

export const g = new TestGroup(UnitTest);

g.test('test/sync', t => {});

g.test('test/async', async t => {});

g.test('testp/sync', t => {
  t.debug(JSON.stringify(t.params));
}).params([{}]);

g.test('testp/async', async t => {
  t.debug(JSON.stringify(t.params));
}).params([{}]);

g.test('testp/private', t => {
  const { a, b, _result } = t.params;
  t.expect(a + b === _result);
}).params([
  { a: 1, b: 2, _result: 3 }, //
  { a: 4, b: -3, _result: 1 },
]);
