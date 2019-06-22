export const description = `
Basic unit tests for test framework.
`;

import { DefaultFixture } from '../framework/default_fixture.js';
import { TestGroup } from '../framework/index.js';

export const group = new TestGroup(DefaultFixture);

group.test('test/sync', null, t => {});

group.test('test/async', null, async t => {});

group.test('testp/sync', {}, t => {
  t.log(JSON.stringify(t.params));
});

group.test('testp/async', {}, async t => {
  t.log(JSON.stringify(t.params));
});
