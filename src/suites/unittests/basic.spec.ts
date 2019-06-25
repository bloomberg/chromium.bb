export const description = `
Basic unit tests for test framework.
`;

import { DefaultFixture } from '../../framework/default_fixture.js';
import { TestGroup } from '../../framework/index.js';

export const g = new TestGroup(DefaultFixture);

g.test('test/sync', t => {});

g.test('test/async', async t => {});

g.test('testp/sync', t => {
  t.log(JSON.stringify(t.params));
}).params([{}]);

g.test('testp/async', async t => {
  t.log(JSON.stringify(t.params));
}).params([{}]);
